#
# Script that tries to find to which function given address belongs. (in a messy way :P)
#

import os
import re
import sys


swosppDir = os.getenv('SWOSPP', r'c:\swospp');
swosDir = os.getenv('SWOS', r'c:\games\swos');
hexRe = '[a-fA-F0-9]+'


def getAddress():
    if len(sys.argv) < 2:
        sys.exit('Usage {} <address>'.format(os.path.basename(sys.argv[0])))
    try:
        return int(sys.argv[1], 0)
    except ValueError:
        try:
            return int(sys.argv[1], 16)
        except:
            sys.exit("Don't know what to do with that address.")


def getBuild():
    build = 'rel';
    if os.path.isfile(os.path.join(swosppDir, 'bin', 'DEBUG')):
        build = 'dbg'
    return build


def getMapFilename(build):
    return os.path.join(swosppDir, 'var', 'swospp_' + build + '.map')


def parseSwosppMapFile(build, address):
    addressRegex = re.compile(r'\s+(?P<address>[a-fA-F0-9]+)\s+(?P<function>\w+)\s+(?P<section>[\w\.]+)\s+(?P<objFile>[\w\.]+)')
    with open(getMapFilename(build)) as mapFile:
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
    lstFilename = os.path.join(swosppDir, 'var', objFile.replace('.obj', '.lst').replace('.cpp', ''))

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
    loadAddrRegex = re.compile((r'SWOS\+\+.*loaded at 0x(?P<swosppLoadAddress>{})'
        '.*SWOS code segment starting at 0x(?P<swosCodeAddress>{})'
        '.*data starting at 0x(?P<swosDataAddress>{})').format(hexRe, hexRe, hexRe))

    try:
        with open(os.path.join(swosDir, 'SWOSPP.LOG')) as logFile:
            for line in logFile.readlines():
                loadAddrMatch = loadAddrRegex.search(line)
                if loadAddrMatch:
                    return int(loadAddrMatch.group('swosppLoadAddress'), 16), int(loadAddrMatch.group('swosCodeAddress'), 16), \
                        int(loadAddrMatch.group('swosDataAddress'), 16)
    except IOError:
        pass

    defaultSwosppLoadAddress = 0x387000
    defaultSwosCodeAddress = 0x220000
    defaultSwosDataAddress = 0x2c1000

    return defaultSwosppLoadAddress, defaultSwosCodeAddress, defaultSwosDataAddress


def getBase(build):
    baseRegex = re.compile(r'(?P<base>{}).*\.text.*PUBLIC\s+USE32\s+PARA'.format(hexRe))

    try:
        with open(os.path.join(swosppDir, 'var', 'swospp_' + build + '.map')) as mapFile:
           for line in mapFile.readlines():
               baseMatch = baseRegex.search(line)
               if baseMatch:
                   return int(baseMatch.group('base'), 16)
    except IOError:
        pass

    return 0x401000


def findSwosAddress(address, isCode):
    addrRegex = re.compile(r'^\s*000{}:(?P<address>{})\s+(?P<symbol>[\w]+)$'.format(('2', '1')[isCode], hexRe))
    closestAddress = -1
    smallestDiff = address + 1
    section = ('data', 'code')[isCode]
    idaOffset = (0xc0000, 0x10000)[isCode]
    idaAddress = address + idaOffset

    with open(os.path.join(swosppDir, 'mapcvt', 'swos.map')) as mapFile:
        for line in mapFile.readlines():
            addrMatch = addrRegex.match(line)
            if addrMatch:
                currentAddress = int(addrMatch.group('address'), 16)
                if currentAddress == address:
                    print(addrMatch.group('symbol'), hex(idaAddress), '[SWOS {}: exact match]'.format(section))
                    return
                elif currentAddress < address and address - currentAddress < smallestDiff:
                    smallestDiff = address - currentAddress
                    closestAddress = currentAddress
                    symbol = addrMatch.group('symbol')

    if closestAddress > 0:
        print('{}+{} {} [SWOS {}]'.format(symbol, hex(smallestDiff), hex(idaAddress), section))
    else:
        print('Address not found.')


def findSwosppAddress(build, address):
    address >= 0 or sys.exit('Address seems to be below SWOS++ area.')

    functionAddress, nextFunctionAddress, functionName, nextFunctionName, objFile, nextObjFile = parseSwosppMapFile(build, address)
    functionAddress or sys.exit('Specified address not found!')

    if nextFunctionAddress == address:
        functionAddress = nextFunctionAddress
        functionName = nextFunctionName
        objFile = nextObjFile
    else:
        newFunctionName = pinpointFunction(address - functionAddress, functionName, objFile)
        if not newFunctionName and objFile != nextObjFile:
            newFunctionName, newFunctionAddress = findNegativeOffsetFunction(nextFunctionAddress - address,
                nextFunctionName, nextFunctionAddress, nextObjFile)
            if newFunctionName:
                functionName = newFunctionName
                objFile = nextObjFile
                functionAddress = newFunctionAddress

    print(hex(functionAddress), '{}{:+#x}'.format(functionName, functionAddress - address), objFile)


def main():
    address = getAddress()
    build = getBuild()

    swosppLoadAddress, swosCodeAddress, swosDataAddress = getLoadAddress()

    belongsTo = ''
    if address >= swosCodeAddress and address < swosDataAddress:
        belongsTo = 'SWOS code'
    elif swosppLoadAddress < swosCodeAddress:
        if address >= swosDataAddress:
            belongsTo = 'SWOS data'
        elif address >= swosppLoadAddress:
            belongsTo = 'SWOS++'
    else:
        if address >= swosppLoadAddress:
            belongsTo = 'SWOS++'
        elif address >= swosDataAddress:
            belongsTo = 'SWOS data'

    belongsTo or sys.exit('Address seems to be below SWOS/SWOS++ area.')

    if belongsTo == 'SWOS code':
        address = address - swosCodeAddress
        findSwosAddress(address, isCode=True)
    elif belongsTo == 'SWOS data':
        address = address - swosDataAddress
        findSwosAddress(address, isCode=False)
    else:
        base = getBase(build)
        address = address - swosppLoadAddress + base
        findSwosppAddress(build, address)


if __name__ == '__main__':
    main()
