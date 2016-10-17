#
# Script that tries to find to which function given address belongs. (in a messy way :P)
#

import os
import re
import sys


swosppDir = os.getenv('SWOSPP', r'f:\swospp');
swosDir = os.getenv('SWOS', r'd:\games\swos');


def getAddress():
    if len(sys.argv) < 2:
        sys.exit('Usage {} <address>'.format(sys.argv[0]))
    return int(sys.argv[1], 0)


def getBuild():
    build = 'rel';
    if os.path.isfile(os.path.join(swosppDir, 'bin', 'DEBUG')):
        build = 'dbg'
    return build


def parseMapFile(build, address):
    addressRegex = re.compile(r'\s+(?P<address>[a-fA-F0-9]+)\s+(?P<function>\w+)\s+(?P<section>[\w\.]+)\s+(?P<objFile>[\w\.]+)')
    with open(os.path.join(swosppDir, 'var', 'swospp_' + build + '.map')) as mapFile:
        prevAddress = 0
        prevFunction = ''
        prevFunctionObjFile = ''
        for line in mapFile.readlines():
            match = addressRegex.match(line)
            if match and match.group('section') == '.text':
                functionAddress = int(match.group('address'), 16)
                functionName = match.group('function')
                functionObjFile = match.group('objFile')
                if functionAddress >= address:
                    if prevAddress == 0:
                        prevAddress = functionAddress
                        prevFunction = functionName
                        prevFunctionObjFile = functionObjFile
                    return prevAddress, functionAddress, prevFunction, functionName, prevFunctionObjFile, functionObjFile
                prevAddress = functionAddress
                prevFunction = functionName
                prevFunctionObjFile = functionObjFile
    return None, None, None, None, None, None


generatedLabelRegex = re.compile(r'(LBL|LVL|LBB|LBE|LFE|LFB|LC|LCOLDB|LHOTB|LCOLDE|LHOTE|L)\d+')

def isGeneratedLabel(label):
    return generatedLabelRegex.match(label)


codeRegex = re.compile(r'\s+\d+\s+(?P<address>[a-fA-F0-9]+)\s+[a-fA-F0-9()]+')
funcRegex = re.compile(r'\s+\d+\s+(?P<functionName>[\w.]+):$')
sectionRegex = re.compile(r'\s+\d+\s+\.?section\s+(?P<sectionName>[\w.]+),("[^"]+")?$')

def pinpointFunction(delta, functionName, objFile):
    lstFilename = os.path.join(swosppDir, 'var', objFile.replace('.obj', '.lst'))

    if not os.path.isfile(lstFilename):
        return None

    with open(lstFilename) as lstFile:
        symbolRegex = re.compile(r'\s+\d+\s+' + functionName + ':$')
        startLooking = False
        counting = False
        currentFunctionName = functionName
        currentSection = ''
        for line in lstFile.readlines():
            sectionMatch = sectionRegex.match(line)
            if sectionMatch:
                currentSection = sectionMatch.group('sectionName')
            if not currentSection.startswith('.text'):
                continue

            if startLooking:
                codeMatch = codeRegex.match(line)
                if codeMatch:
                    startAddress = int(codeMatch.group('address'), 16)
                    startLooking = False
                    counting = True
            elif counting:
                funcMatch = funcRegex.match(line)
                if funcMatch and not isGeneratedLabel(funcMatch.group('functionName')):
                    currentFunctionName = funcMatch.group('functionName')
                    continue
                else:
                    codeMatch = codeRegex.match(line)
                    if codeMatch:
                        address = int(codeMatch.group('address'), 16)
                        if address - startAddress >= delta:
                            return currentFunctionName
            else:
                symMatch = symbolRegex.match(line)
                if symMatch:
                    startLooking = True

    return None


def findNegativeOffsetFunction(delta, functionName, functionAddress, objFile):
    symbolRegex = re.compile(r'\s+\d+\s+' + functionName + ':$')
    lstFilename = os.path.join(swosppDir, 'var', objFile.replace('.obj', '.lst'))

    if not os.path.isfile(lstFilename):
        return None

    with open(lstFilename) as lstFile:
        currentSection = ''
        currentFunction = ''
        currentOffset = 0
        currentFunctionOffset = 0
        lineToLabel = {}
        offsetToFunctionStart = {}
        for line in lstFile.readlines():
            sectionMatch = sectionRegex.match(line)
            if sectionMatch:
                currentSection = sectionMatch.group('sectionName')
            if not currentSection.startswith('.text'):
                continue

            codeMatch = codeRegex.match(line)
            if codeMatch:
                currentOffset = int(codeMatch.group('address'), 16)
                lineToLabel[currentOffset] = currentFunction
                offsetToFunctionStart[currentOffset] = currentFunctionOffset
            else:
                symMatch = symbolRegex.match(line)
                if symMatch:
                    start = currentOffset - delta
                    while start <= currentOffset:
                        if start in lineToLabel:
                            return lineToLabel[start], functionAddress - (currentOffset - offsetToFunctionStart[start])
                        start += 1
                    return functionName, functionAddress
                else:
                    funcMatch = funcRegex.match(line)
                    if funcMatch and not isGeneratedLabel(funcMatch.group('functionName')):
                        currentFunction = funcMatch.group('functionName')
                        currentFunctionOffset = currentOffset
    return None, None


def getLoadAddress():
    defaultLoadAddress = 0x387000
    swosppLoadAddrRegex = re.compile(r'SWOS\+\+.*loaded at 0x(?P<loadAddress>[a-fA-F0-9]+)')
    try:
        with open(os.path.join(swosDir, 'SWOSPP.LOG')) as logFile:
            for line in logFile.readlines():
                loadAddrMatch = swosppLoadAddrRegex.search(line)
                if loadAddrMatch:
                    return int(loadAddrMatch.group('loadAddress'), 16)
    except FileException:
        return defaultLoadAddress

    return defaultLoadAddress


def main():
    address = getAddress()
    build = getBuild()

    loadAddress = getLoadAddress()
    address = address - loadAddress + 0x401000  # this oughta be read from map file too
    address >= 0 or sys.exit('Address seems to be below SWOS++ area.')

    functionAddress, nextFunctionAddress, functionName, nextFunctionName, objFile, nextObjFile = parseMapFile(build, address)
    functionAddress or sys.exit('Specified address not found!')

    if nextFunctionAddress == address:
        functionAddress = nextFunctionAddress
        functionName = nextFunctionName
        objFile = nextObjFile
    else:
        newFunctionName = pinpointFunction(address - functionAddress, functionName, objFile)
        if not newFunctionName and objFile != nextObjFile:
            newFunctionName, newFunctionAddress = findNegativeOffsetFunction(nextFunctionAddress - address, nextFunctionName, nextFunctionAddress, nextObjFile)
            if newFunctionName:
                functionName = newFunctionName
                objFile = nextObjFile
                functionAddress = newFunctionAddress

    print(hex(functionAddress), functionName, objFile)


if __name__ == '__main__':
    main()
