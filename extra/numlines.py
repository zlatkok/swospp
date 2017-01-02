""" Primitive line counter for measuring progress. """

# Zlatko Karakas, 17.07.2015.

import os
import re
import sys
import string
import operator
import collections
from enum import Enum


class SrcFile:
    def __init__(self, path, type, category):
        self.path = path
        self.type = type
        self.category = category

        self.size = os.path.getsize(path)
        self.totalLines = 0
        self.commentedLines = 0
        self.uncommentedLines = 0
        self.emptyLines = 0

    def getPath(self):
        return self.path

    def getCategory(self):
        return self.category

    def getSize(self):
        return self.size

    def addLine(self):
        self.totalLines += 1

    def addUncommentedLine(self):
        self.uncommentedLines += 1

    def addCommentedLine(self):
        self.commentedLines += 1

    def addEmptyLine(self):
        self.emptyLines += 1

    def getTotalLines(self):
        return self.totalLines

    def getCommentedLines(self):
        return self.commentedLines

    def getEmptyLines(self):
        return self.getEmptyLines

    def __str__(self):
        return '{}, total lines: {}, uncommented lines: {}, commented lines: {}, empty lines: {}'.format(self.path,
            self.totalLines, self.uncommentedLines, self.commentedLines, self.emptyLines)

    def __repr__(self):
        return self.__str__()


CppParserState = Enum('CppParserState', 'code oneLineComment blockComment string charConstant')


codeSwitchStateRegex = re.compile('[/"\']')

def cppSkipCode(line, i, state):
    while i < len(line):
        match = codeSwitchStateRegex.search(line, i)
        if match:
            start = match.start()
            if match.group() == '/':
                if line[start + 1 : start + 2] == '*':
                    return match.start() + 2, CppParserState.blockComment
                elif line[start + 1 : start + 2] == '/':
                    return match.start() + 2, CppParserState.oneLineComment
            elif match.group() == '"':
                return match.start() + 1, CppParserState.string
            elif match.group() == "'":
                return match.start() + 1, CppParserState.charConstant
            i = match.start() + 1
        else:
            break

    return len(line), CppParserState.code


def cppSkipOneLineComment(line, i, state):
    if line[-1] == '\\':
        return len(line), CppParserState.oneLineComment
    else:
        return len(line), CppParserState.code


def cppSkipBlockComment(line, i, state):
    while i < len(line):
        index = line.find('*', i)
        if index >= 0:
            i = index + 1
            if line[i : i + 1] == '/':
                return i + 1, CppParserState.code
        else:
            break

    return len(line), CppParserState.blockComment


def cppSkipEscape(line, i):
    if i < len(line):
        if line[i] in { 'a', 'b', 'f', 'n', 'r', 't', 'v', "'", '"', '\\', '?', None }:
            i += 1
        elif line[i] == 'x':
            while i + 1 < len(line) and line[i + 1] in string.hexdigits:
                i += 1
        else:
            while i < len(line) and line[i] in string.octdigits:
                i += 1

    return i


stringRegex = re.compile(r'[\\"]')
charConstRegex = re.compile(r"[\\']")

def cppSkipStringOrCharConstant(regex, quoteChar, line, i, state):
    while i < len(line):
        match = regex.search(line, i)
        if match:
            if match.group() == '\\':
                i = cppSkipEscape(line, match.start() + 1)
            elif match.group() == quoteChar:
                return match.start() + 1, CppParserState.code
        else:
            break

    return len(line), CppParserState.string


def cppSkipString(line, i, state):
    return cppSkipStringOrCharConstant(stringRegex, '"', line, i, state)


def cppSkipCharConstant(line, i, state):
    return cppSkipStringOrCharConstant(charConstRegex, "'", line, i, state)


CppParserStateFunctions = {
    CppParserState.code: cppSkipCode,
    CppParserState.oneLineComment: cppSkipOneLineComment,
    CppParserState.blockComment: cppSkipBlockComment,
    CppParserState.string: cppSkipString,
    CppParserState.charConstant: cppSkipCharConstant,
}


def parseCppLine(line, state):
    allStates = set()
    i = 0

    while i < len(line):
        i, state = CppParserStateFunctions[state](line, i, state)
        allStates.add(state)

    return state, allStates


def cppParser(srcFile, f):
    state = CppParserState.code
    for line in f:
        srcFile.addLine()
        line = line.strip()

        if not line:
            srcFile.addEmptyLine()
            srcFile.addUncommentedLine()
        else:
            state, lineStates = parseCppLine(line, state)
            if { CppParserState.oneLineComment, CppParserState.blockComment } & lineStates:
                srcFile.addCommentedLine()
            else:
                srcFile.addUncommentedLine()


def oneCharCommentParser(srcFile, f, commentChar):
    for line in f:
        srcFile.addLine()
        line = line.strip()

        if not line:
            srcFile.addEmptyLine()
        elif line[0] == commentChar:
            srcFile.addCommentedLine()
        else:
            srcFile.addUncommentedLine()


def asmParser(srcFile, f):
    oneCharCommentParser(srcFile, f, ';')

def perlParser(srcFile, f):
    oneCharCommentParser(srcFile, f, '#')

def pythonParser(srcFile, f):
    oneCharCommentParser(srcFile, f, '#')


Categories = Enum('Categories', 'cpp asm perl python')

extCategories = {
    'c':    Categories.cpp,
    'cpp':  Categories.cpp,
    'h':    Categories.cpp,
    'asm':  Categories.asm,
    'inc':  Categories.asm,
    'pl':   Categories.perl,
    'py':   Categories.python,
}

categoryParsers = {
    Categories.cpp: cppParser,
    Categories.asm: asmParser,
    Categories.perl: perlParser,
    Categories.python: pythonParser,
}

categoryDisplayName = {
    Categories.cpp: 'C++',
    Categories.asm: 'Assembler',
    Categories.perl: 'Perl',
    Categories.python: 'Python',
}


def getDir():
    if len(sys.argv) > 1:
        return sys.argv[1]
    return os.getenv('SWOSPP', '.')


def getFiles(root):
    generatedFiles = { 'swos.asm', 'swos.inc', 'swossym.h', }
    files = []

    for dirpath, dirnames, filenames in os.walk(root):
        for file in filenames:
            if file not in generatedFiles:
                ext = os.path.splitext(file)[1][1:].lower()
                category = extCategories.get(ext)
                if category:
                    path = os.path.join(dirpath, file)
                    files.append(SrcFile(path, ext, category))

    return files


def processFile(file):
    with open(file.getPath()) as f:
        categoryParsers[file.getCategory()](file, f)

def countLines(files):
    for file in files:
        processFile(file)


def formatFileSize(size):
    origSize = size
    for unit in [ '', 'kb', 'mb', 'gb', 'tb', 'pb' ]:
        if abs(size) < 1024.0:
            return '{:.2f}{}'.format(size, unit)
        size /= 1024.0
    return '{}b'.format(origSize)


def displayReport(files):
    totalLines = 0
    totalBytes = 0
    linesPerCategory = collections.defaultdict(int)
    bytesPerCategory = collections.defaultdict(int)
    commentedPerCategory = collections.defaultdict(int)

    for file in files:
        totalLines += file.getTotalLines()
        totalBytes += file.getSize()
        linesPerCategory[file.getCategory()] += file.getTotalLines()
        bytesPerCategory[file.getCategory()] += file.getSize()
        commentedPerCategory[file.getCategory()] += file.getCommentedLines()

    sortedLines = sorted(linesPerCategory.items(), key=operator.itemgetter(1), reverse=True)

    print("Total lines: {} [{}]".format(totalLines, formatFileSize(totalBytes)))
    print("------------------")
    for category, lines in sortedLines:
        print('{:10}: {:5} {:<6.2%} [{:>8}], lines with comments: {:4} ({:.2%})'.format(categoryDisplayName[category],
            lines, lines / totalLines, formatFileSize(bytesPerCategory[category]), commentedPerCategory[category], commentedPerCategory[category] / totalLines ))


def main():
    dir = getDir()
    files = getFiles(dir)
    try:
        countLines(files)
        displayReport(files)
    except FileNotFoundError as e:
        print(e)


if __name__ == '__main__':
    main()
