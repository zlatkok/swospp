#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "types.h"
#define sprintf __sprintf
#include "swos.h"
#define rand __rand
#define strtol __strtol
#include "util.h"
#undef rand
#include "xmltree.h"
#include "xmlparse.h"
#include "qalloc.h"
#include "bfile_mock.h"
#include "dos.h"

/* can't include Windows.h or all hell will break loose */
extern unsigned long __cdecl _exception_code(void);
extern void * __cdecl _exception_info(void);

extern void TurnLogOn();
extern void TurnLogOff();

typedef struct RunResult {
    XmlSymbol sym;
    char *text;
} RunResult;

#undef  EOF
#define OPEN_START_TAG  { XML_SYM_TAG_OPEN, nullptr }
#define CLOSE_START_TAG { XML_SYM_TAG_CLOSE, nullptr }
#define CLOSE_SOLO_TAG  { XML_SYM_TAG_CLOSE_SOLO_TAG, nullptr }
#define OPEN_END_TAG    { XML_SYM_TAG_OPEN_END_TAG, nullptr }
#define TEXT(a)         { XML_SYM_TEXT, (a) }
#define EQUAL           { XML_SYM_EQ, nullptr }
#define EOF             { XML_SYM_EOF, nullptr }
#define END             { XML_SYM_MAX, nullptr }

static void verify(const RunResult *results, XmlSymbol sym, char *buf, int bufSize)
{
    static int count;
    assert(results[count].sym != XML_SYM_MAX);
    assert(sym == results[count].sym);
    if (sym == XML_SYM_TEXT)
        assert(sym == results[count].sym && !memcmp(buf, results[count].text, bufSize));
    count++;
    if (sym == XML_SYM_EOF) {
        count = 0;
        TurnLogOff();
    }
}

static bool lexTest00(XmlSymbol sym, char *buf, int bufSize)
{
    static const RunResult results[] = {
        OPEN_START_TAG,
        TEXT("!-"),
        OPEN_START_TAG,
        TEXT("!"),
        EOF,
        END,
    };
    verify(results, sym, buf, bufSize);
    return true;
}

static bool lexTest01(XmlSymbol sym, char *buf, int bufSize)
{
    static const RunResult results[] = {
        TEXT("hanging &te#x$\\t\r\n\r\n&"),
        EOF,
        END,
    };
    verify(results, sym, buf, bufSize);
    return true;
}

static bool lexTest02(XmlSymbol sym, char *buf, int bufSize)
 {
    static const RunResult results[] = {
        TEXT("<< \"'&'&'\" >> &quo"),
        EOF,
        END,
    };
    verify(results, sym, buf, bufSize);
    return true;
}

static bool lexTest03(XmlSymbol sym, char *buf, int bufSize)
{
    static const RunResult results[] = {
        TEXT("whitespace removal around free text"),
        EOF,
        END,
    };
    verify(results, sym, buf, bufSize);
    return true;
}

static bool lexTest04(XmlSymbol sym, char *buf, int bufSize)
{
    static const RunResult results[] = {
        TEXT(" a123  \xab""a037D\\\\x*\xfe\xff\x00\x01\xd4\xe7\x59rr54\\x\t\r\n\r\b\\\\\r\n    a\tak4\\t456"),
        EOF,
        END,
    };
    verify(results, sym, buf, bufSize);
    return true;
}

static bool lexTest05(XmlSymbol sym, char *buf, int bufSize)
{
    static const RunResult results[] = {
        OPEN_START_TAG,
        TEXT("SpacePirates"),
        TEXT("type"),
        EQUAL,
        TEXT("string"),
        TEXT("Remix"),
        EQUAL,
        TEXT("\tGhostbusters\t"),
        CLOSE_START_TAG,
        TEXT("gamemusicparadise"),
        OPEN_END_TAG,
        TEXT("SpacePirates"),
        CLOSE_START_TAG,
        EOF,
        END,
    };
    verify(results, sym, buf, bufSize);
    return true;
}

static bool lexTest06(XmlSymbol sym, char *buf, int bufSize)
{
    static const RunResult results[] = {
        OPEN_START_TAG,
        TEXT("node"),
        TEXT("attr"),
        EQUAL,
        TEXT("string"),
        TEXT("attr2"),
        EQUAL,
        TEXT("another string"),
        TEXT("attr3"),
        EQUAL,
        TEXT("no-strings-attached"),
        CLOSE_START_TAG,
        TEXT("Some content, \"with strings too\""),
        OPEN_END_TAG,
        TEXT("node"),
        CLOSE_START_TAG,
        OPEN_START_TAG,
        TEXT("single"),
        TEXT("ondelete"),
        EQUAL,
        TEXT("fast_delete()"),
        TEXT("alloc_type"),
        EQUAL,
        TEXT("fast"),
        CLOSE_SOLO_TAG,
        OPEN_START_TAG,
        TEXT("chapterMarker"),
        CLOSE_SOLO_TAG,
        OPEN_START_TAG,
        TEXT("stop"),
        CLOSE_START_TAG,
        OPEN_END_TAG,
        TEXT("stop"),
        CLOSE_START_TAG,
        OPEN_START_TAG,
        TEXT("formOfLife"),
        TEXT("significance"),
        EQUAL,
        TEXT("insignificant"),
        CLOSE_SOLO_TAG,
        OPEN_START_TAG,
        TEXT("message"),
        TEXT("from"),
        EQUAL,
        TEXT("another_time"),
        CLOSE_START_TAG,
        TEXT
        (
            "Some more text, and binary strings:\n"
            "\x4c\x61\x73\x65\x72\x64\x61\x6e\x63\x65"
            "\x20\x2d\x20\x4d\x6f\x6f\x6e\x20\x4d\x61"
            "\x63"  "\x68\x69"  "\x6e"  "\x65"  "\x00"
        ),
        OPEN_END_TAG,
        TEXT("message"),
        CLOSE_START_TAG,
        EOF,
        END,
    };
    verify(results, sym, buf, bufSize);
    return true;
}

static bool lexTest07(XmlSymbol sym, char *buf, int bufSize)
{
    static const RunResult results[] = {
        OPEN_START_TAG,
        TEXT("tagZorg"),
        CLOSE_START_TAG,
        OPEN_END_TAG,
        TEXT("tagZorg"),
        CLOSE_START_TAG,
        EOF,
        END,
    };
    verify(results, sym, buf, bufSize);
    return true;
}

/** generateRandomContents

    fileName -> file name to which write generated xml

    Writes random xml content to file, and returns array of results that corresponds to that content.
*/
static RunResult *generateRandomContents(const char *fileName)
{
    int i, j;
    XmlSymbol sym = XML_SYM_MAX, lastSym;
    bool inTag = false;
    static RunResult results[3000];
    char fileBuf[1024], symBuf[2];
    int strIndex;
    int count;
    int charsBeforeNewLine = 0;
    int minSpaces;
    int lastDigitEscape = -2;

    BFile *file = CreateBFile(F_WRITE_ONLY, fileName);
    if (!file)
        return nullptr;
    for (i = 0; i < sizeofarray(results) - 1; i++) {
        lastSym = sym;
        sym = rand() % XML_SYM_MAX;
        if (lastSym == XML_SYM_TEXT && sym == XML_SYM_TEXT && !inTag)
            sym = XML_SYM_TAG_OPEN;
        /* don't nest tags... altho it might be good to test that case too? */
        if (sym == XML_SYM_EOF || inTag && (sym == XML_SYM_TAG_OPEN || sym == XML_SYM_TAG_OPEN_END_TAG) ||
            !inTag && sym == XML_SYM_EQ)
            sym = XML_SYM_TEXT;
        if (lastSym == XML_SYM_TEXT && sym == XML_SYM_TEXT)
            sym = XML_SYM_TAG_CLOSE;
        assert(sym != XML_SYM_TEXT || lastSym != XML_SYM_TEXT);
        results[i].sym = sym;
        results[i].text = nullptr;
        strIndex = 0;
        if (sym == XML_SYM_TEXT) {
            int stringLen = 2 + rand() % 128;
            bool quoted = rand() & 1;
            if (quoted)
                stringLen = max(2, stringLen);
            results[i].text = malloc(stringLen);
            if (quoted)
                fileBuf[0] = '"';
            /* fileBuf[strIndex] what goes to file, results[i].text[j] what will it become in memory */
            for (j = 0, strIndex = quoted; j < stringLen; j++, strIndex++) {
                unsigned char c = rand() % 256;
                if (c == 5 || c == 6 || c == 18 || c == 19) {   /* 1 in 64 chance */
                    /* generate random escape */
                    static const char *escapes[] = { "<\002lt",  ">\002gt", "&\003amp", "'\004apos", "\"\004quot" };
                    int whichEscape = rand() % sizeofarray(escapes);
                    int escLen = escapes[whichEscape][1];
                    results[i].text[j] = escapes[whichEscape][0];
                    fileBuf[strIndex] = '&';
                    memcpy(fileBuf + strIndex + 1, escapes[whichEscape] + 2, escLen);
                    fileBuf[strIndex + 1 + escLen] = ';';
                    strIndex += escLen + 1;
                    continue;
                } else if (c < 32 || c > 126) {
                    /* increase chance of getting printable characters */
                    if (rand() % 8) {
                        c = 32 + rand() % 95;
                    } else {
                        if (quoted) {
                            results[i].text[j] = c;
                            fileBuf[strIndex] = '\\';
                            if (rand() & 1) {
                                fileBuf[strIndex + 1] = 'x';
                                fileBuf[strIndex + 2] = ((c & 0xf0) >> 4) + (((c & 0xf0) >> 4) > 9 ? 'a' - 10 : '0');
                                fileBuf[strIndex + 3] = (c & 0x0f) + ((c & 0x0f) > 9 ? 'a' - 10 : '0');
                            } else {
                                fileBuf[strIndex + 1] = c / 100 + '0';
                                fileBuf[strIndex + 2] = (c / 10) % 10 + '0';
                                fileBuf[strIndex + 3] = c % 10 + '0';
                                lastDigitEscape = j;
                            }
                            strIndex += 3;
                            continue;
                        } else
                            c = '.';
                    }
                }
                assert(c >= 23 && c <= 126);
                if (!quoted) {
                    if (isspace(c))
                        c = '^';
                    else if (c == '>')
                        c = '*';
                    else if (c == '<')
                        c = '|';
                    else if (c == '"')
                        c = '\'';
                    else if (c == '=' && inTag)
                        c = '_';
                    else if (c == '&')
                        c = '%';
                    else if (c == '/' && (lastSym == XML_SYM_TAG_OPEN || j == stringLen - 1))
                        c = '[';
                } else {
                    if (c == '\\')
                        c = '|';
                    else if (c == '"')
                        c = '\'';
                    else if (isdigit(c) && lastDigitEscape == j - 1)
                        c = '~';
                }
                fileBuf[strIndex] = results[i].text[j] = c;
            }
            if (quoted)
                fileBuf[strIndex++] = '"';
            assert(strIndex <= sizeof(fileBuf));
            for (j = 0; j < strIndex; j++)
                assert(fileBuf[j] >= 32 && fileBuf[j] <= 126);
        }
        switch (sym) {
        case XML_SYM_TAG_OPEN:
            symBuf[0] = '<';
            count = 1;
            inTag = true;
            break;
        case XML_SYM_TAG_OPEN_END_TAG:
            symBuf[0] = '<';
            symBuf[1] = '/';
            count = 2;
            inTag = true;
            break;
        case XML_SYM_TAG_CLOSE:
            symBuf[0] = '>';
            count = 1;
            inTag = false;
            break;
        case XML_SYM_TAG_CLOSE_SOLO_TAG:
            j = 0;
            if (lastSym == XML_SYM_TAG_OPEN) {
                symBuf[0] = ' ';
                j = 1;
            }
            symBuf[j] = '/';
            symBuf[j + 1] = '>';
            count = j + 2;
            inTag = false;
            break;
        case XML_SYM_EQ:
            symBuf[0] = '=';
            count = 1;
            break;
        case XML_SYM_TEXT:
            break;
        default:
            assert_msg(0, "new xml symbol added and you didn't tell me?");
        }
        if (sym != XML_SYM_TEXT) {
            WriteBFile(file, symBuf, count);
            charsBeforeNewLine += count;
        }
        /* add random whitespace */
        minSpaces = 0;
        if (lastSym == XML_SYM_TEXT && !inTag)
            minSpaces = 1;
        count = minSpaces + rand() % (min(sym == XML_SYM_TEXT ? 32 : 3, sizeof(fileBuf) - strIndex) - minSpaces);
        assert(count < sizeof(fileBuf) - strIndex + 1);
        if (count > 0) {
            memset(fileBuf + strIndex, ' ', count - 1);
            if (charsBeforeNewLine > 90) {
                charsBeforeNewLine = 0;
                fileBuf[strIndex + count - 1] = '\n';
            } else
                fileBuf[strIndex + count - 1] = '\t';
        }
        charsBeforeNewLine += strIndex + count;
        WriteBFile(file, fileBuf, strIndex + count);
    }
    results[sizeofarray(results) - 1].sym = XML_SYM_EOF;
    results[sizeofarray(results) - 1].text = nullptr;
    CloseBFile(file);
    return results;
}

static RunResult *randomResults;
static const char *randomFileName = "..\\xml\\lex-random.xml";

static bool randTest(XmlSymbol sym, char *buf, int bufSize)
{
    assert(randomResults);
    verify(randomResults, sym, buf, bufSize);
    return true;
}

//#define CUSTOM_LEX_TEST
#ifdef CUSTOM_LEX_TEST
static bool customTest(XmlSymbol sym, char *buf, int bufSize)
{
    static int count;
    if (count == 1951) {
        _asm int 3;
    }
    count++;
    return true;
}
#endif

#define IS_LEAF         true
#define IS_NOT_LEAF     false

#define MATCH_NODE(n, name, type, isLeaf)   {               \
    assert(n && name && !strcmp(XmlNodeGetName(n), name));  \
    assert(XmlNodeGetType(n) == type);                      \
    assert(XmlNodeIsLeaf(n) == isLeaf);                     \
}

#define MATCH_NODE_NAME(n, name)    { assert(!strcmp(GetXmlNodeName(n), name)); }
#define MATCH_NODE_STR(n, str)      { assert(!strcmp(XmlNodeGetString(n), str)); }
#define MATCH_NODE_CHAR(n, ch)      { assert(XmlNodeGetChar(n) == (ch)); }
#define MATCH_NODE_SHORT(n, sh)     { assert(XmlNodeGetShort(n) == (sh)); }
#define MATCH_NODE_INT(n, i)        { assert(XmlNodeGetInt(n) == (i)); }
#define MATCH_NODE_TYPE(n, type)    { assert(XmlNodeGetType(n) == (type)); }
#define MATCH_NON_LEAF_NODE(n)      { assert(!XmlNodeIsLeaf(n)); }
#define MATCH_LEAF_NODE(n)          { assert(XmlNodeIsLeaf(n)); }

#define MATCH_NODE_ATTR(n, attr, val) {                                 \
    const char *attrVal;                                                \
    size_t attrValLen;                                                  \
    assert(attr && val);                                                \
    attrVal = GetXmlNodeAttribute(n, attr, strlen(attr), &attrValLen);  \
    assert(attrVal);                                                    \
    assert(!strcmp(attrVal, val));                                      \
}

static bool verifyParseTree00(const XmlNode *tree)
{
    const XmlNode *n = tree;
    MATCH_NODE(n, "Yngwie", XML_EMPTY, IS_NOT_LEAF);
    n = XmlNodeGetChildren(n);

        MATCH_NODE(n, "charTag1", XML_CHAR, IS_LEAF);
        MATCH_NODE_CHAR(n, -99);
        n = XmlNodeGetNextSibling(n);

        MATCH_NODE(n, "charTag2", XML_CHAR, IS_LEAF);
        MATCH_NODE_CHAR(n, 24);
        n = XmlNodeGetNextSibling(n);

        MATCH_NODE(n, "shortTag1", XML_SHORT, IS_LEAF);
        MATCH_NODE_SHORT(n, -25000);
        n = XmlNodeGetNextSibling(n);

        MATCH_NODE(n, "shortTag2", XML_SHORT, IS_LEAF);
        MATCH_NODE_SHORT(n, 6072);
        n = XmlNodeGetNextSibling(n);

        MATCH_NODE(n, "intTag1", XML_INT, IS_LEAF);
        MATCH_NODE_INT(n, -5000000);
        n = XmlNodeGetNextSibling(n);

        MATCH_NODE(n, "intTag2", XML_INT, IS_LEAF);
        MATCH_NODE_INT(n, LONG_MIN);

    return true;
}

static bool verifyParseTree01(const XmlNode *tree)
{
    const XmlNode *n = tree, *n2;
    MATCH_NODE(n, "Krynn", XML_EMPTY, IS_NOT_LEAF);
    n = XmlNodeGetChildren(n);

        MATCH_NODE(n, "node", XML_STRING, IS_NOT_LEAF);
        MATCH_NODE_ATTR(n, "attr", "string");
        MATCH_NODE_ATTR(n, "attr2", "another string");
        MATCH_NODE_ATTR(n, "attr3", "no-strings-attached");
        MATCH_NODE_ATTR(n, "type", "string");
        MATCH_NODE_STR(n, "Some content, \"with strings too\". This should get concatenated.");

        n2 = XmlNodeGetChildren(n);
            MATCH_NODE(n2, "someTag", XML_EMPTY, IS_NOT_LEAF);
            MATCH_NODE_ATTR(n2, "type", "empty");

                n2 = XmlNodeGetChildren(n2);
                MATCH_NODE(n2, "miniMe", XML_EMPTY, IS_LEAF);

        n = XmlNodeGetNextSibling(n);
        MATCH_NODE(n, "single", XML_EMPTY, IS_LEAF);
        MATCH_NODE_ATTR(n, "ondelete", "fast_delete()");
        MATCH_NODE_ATTR(n, "alloc_type", "fast");

        n = XmlNodeGetNextSibling(n);
        MATCH_NODE(n, "chapterMarker", XML_EMPTY, IS_LEAF);

        n = XmlNodeGetNextSibling(n);
        MATCH_NODE(n, "stop", XML_EMPTY, IS_LEAF);

        n = XmlNodeGetNextSibling(n);
        MATCH_NODE(n, "formOfLife", XML_EMPTY, IS_LEAF);
        MATCH_NODE_ATTR(n, "significance", "insignificant");

        n = XmlNodeGetNextSibling(n);
        MATCH_NODE(n, "message", XML_STRING, IS_LEAF);
        MATCH_NODE_ATTR(n, "from", "another_time");
        MATCH_NODE_ATTR(n, "type", "string");
        MATCH_NODE_STR(n, "Some more text, and binary strings:\n"
            "\x4c\x61\x73\x65\x72\x64\x61\x6e\x63\x65"
            "\x20\x2d\x20\x4d\x6f\x6f\x6e\x20\x4d\x61"
            "\x63"  "\x68\x69"  "\x6e"  "\x65"  "\x00"
        );
    return true;
}

static int getXmlNodeTypeLength(XmlNodeType type)
{
    switch (type) {
    case XML_CHAR:
        return 1;
    case XML_SHORT:
        return 2;
    case XML_INT:
        return 4;
    case XML_EMPTY:
        return 0;
    case XML_STRING:
    case XML_ARRAY:
        return 16 + rand() % 128;
    default:
        assert_msg(0, "Uknown xml node type to generate length for.");
    }
    return 0;
}

static const char *generateRandomString(int length)
{
    const char allowedChars[] = {
        '-', '_', '_', '_', '_', '_',
        '<', '>', '\n', '\t', '=',
        'A', 'E', 'E', 'I', 'O', 'U', 'R', 'S', 'T',
        'A', 'E', 'I', 'O', 'U', 'R',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    };
    static char randStrBuf[512];
    int i;
    assert(length < sizeof(randStrBuf));
    for (i = 0; i < length; i++)
        randStrBuf[i] = allowedChars[rand() % sizeofarray(allowedChars)];
    randStrBuf[i] = '\0';
    return randStrBuf;
}

static bool generateRandomContent(XmlNode *node)
{
    assert(node);
    switch (node->type) {
    case XML_CHAR:
        assert(node->length == 1);
        *node->value.charVal = rand() % 256;
        break;
    case XML_SHORT:
        assert(node->length == 2);
        *node->value.shortVal = rand() % (64 * 1024);
        break;
    case XML_INT:
        assert(node->length == 4);
        *node->value.intVal = rand() * INT_MAX / RAND_MAX;
        break;
    case XML_STRING:
    case XML_ARRAY:
        assert(node->value.ptr);
        memcpy(node->value.ptr, generateRandomString(node->length), node->length);
        if (node->type == XML_STRING)
            node->value.ptr[node->length - 1] = '\0';
        break;
    }
    return true;
}

static XmlNode *generateRandomNode()
{
    XmlNode *node;
    int type, length, nameLength;
    static const XmlNodeType types[] = {
        XML_CHAR,
        XML_SHORT,
        XML_INT,
        XML_STRING,
        XML_ARRAY,
        XML_EMPTY,
    };
    int numAttrs = rand() % 4;

    type = rand() % sizeofarray(types);
    length = getXmlNodeTypeLength(types[type]);
    nameLength = 3 + rand() % 16;
    if (!(node = NewXmlNode(generateRandomString(nameLength), nameLength, types[type], length, true)))
        return nullptr;
    while (numAttrs--) {
        char attrNameBuf[128];
        int attrNameLen = 4 + rand() % 8;
        int attrValLen = 8 + rand() % 16;
        memcpy(attrNameBuf, generateRandomString(attrNameLen), attrNameLen + 1);
        /* ignore failure */
        AddXmlNodeAttribute(node, attrNameBuf, attrNameLen, generateRandomString(attrValLen), attrValLen);
    }
    if (types[type] != XML_EMPTY) {
        size_t typeNameLength;
        const char *typeName = XmlNodeTypeToString(types[type], &typeNameLength);
        if (!AddXmlNodeAttribute(node, "type", 4, typeName, typeNameLength))
            return nullptr;
    }
    generateRandomContent(node);
    return node;
}

static XmlNode *generateTestTree(int numNodes)
{
    XmlNode *rootNode = generateRandomNode();
    int numChildren = 1 + rand() % 8;

    if (!rootNode)
        return nullptr;

    --numNodes;
    if (numChildren = min(numChildren, numNodes)) {
        int nodesPerChild = numNodes / numChildren;
        int remainder = numNodes - numChildren * nodesPerChild;
        while (numChildren--) {
            XmlNode *child = generateTestTree(nodesPerChild + (remainder-- > 0));
            if (!child)
                return nullptr;
            AddXmlNode(rootNode, child);
        }
    }

    return rootNode;
}

static bool XmlNodesEqual(const XmlNode *node1, const XmlNode *node2)
{
    if (!node1 ^ !node2)
        return false;
    if (!node1)
        return true;
    if (XmlNodeIsLeaf(node1) ^ XmlNodeIsLeaf(node2))
        return false;
    if (node1->nameHash != node2->nameHash || node1->nameLength != node2->nameLength ||
        strcmp(node1->name, node2->name))
        return false;
    if (node1->numAttributes != node2->numAttributes)
        return false;
    if (node1->attributes) {
        XmlAttribute *attr1 = node1->attributes;
        while (attr1) {
            size_t valLen;
            const char *val = GetXmlNodeAttribute(node2, attr1->name, attr1->nameLength, &valLen);
            if (!val || attr1->valueLength != valLen || memcmp(attr1->value, val, valLen))
                return false;
            attr1 = attr1->next;
        }
    }
    if (XmlNodeGetType(node1) != XmlNodeGetType(node2))
        return false;
    if (node1->length != node2->length)
        return false;
    switch (node1->type) {
    case XML_CHAR:
        return *node1->value.charVal == *node2->value.charVal;
    case XML_SHORT:
        return *node1->value.shortVal == *node2->value.shortVal;
    case XML_INT:
        return *node1->value.intVal == *node2->value.intVal;
    case XML_STRING:
    case XML_ARRAY:
        return !memcmp(node1->value.ptr, node2->value.ptr, node1->length);
    }
    return true;
}

static bool XmlTreesEqual(const XmlNode *tree1, const XmlNode *tree2)
{
    if (!XmlNodesEqual(tree1, tree2))
        return false;
    if (tree1 && !XmlNodeIsLeaf(tree1)) {
        const XmlNode *tree1Child = tree1->children;
        while (tree1Child) {
            const XmlNode *tree2Child = tree2->children;
            while (tree2Child) {
                if (XmlNodesEqual(tree1Child, tree2Child))
                    break;
                tree2Child = tree2Child->nextChild;
            }
            if (!tree2Child)
                return false;
            if (!XmlTreesEqual(tree1Child->children, tree2Child->children))
                return false;
            tree1Child = tree1Child->nextChild;
        }
    }
    return true;
}

int main()
{
    char xmlLexTestFile[] = "..\\xml\\lex-test-00.xml";
    char parseNegTestFile[] = "..\\xml\\parse-neg-test-00.xml";
    static const char *parseRandomTestFile = "..\\xml\\parse-random.xml";

    const int numParseNegTestFiles = 14;
    int i;
    XmlNode *tree;
    static const XmlSymbolProcessingFunction testFunctions[] = {
        lexTest00, lexTest01, lexTest02, lexTest03, lexTest04, lexTest05, lexTest06, lexTest07
    };

    TurnLogOn();
    qAllocInit();
    atexit((void (*)())getchar);
    atexit(qAllocFinish);
    srand(time(nullptr));

    SetupWinFileEmulationLayer((uint8_t *)OpenBFile, (uint8_t *)OpenBFile + 3 * 1024);
    __try {
#ifdef CUSTOM_LEX_TEST
        LoadXmlFile(nullptr, "..\\xml\\random1.xml", customTest);
#endif
        /* test lexer first, load files from xml dir named lex-test-00.xml, lex-test-01.xml... and so on */
        i = 0;
        do {
            XmlSymbolProcessingFunction symProc = nullptr;
            if (i < sizeofarray(testFunctions) && testFunctions[i])
                symProc = testFunctions[i];
            printf("Lex test: %s...\n", xmlLexTestFile + 7);
            TurnLogOn();
            if (!LoadXmlFile(nullptr, xmlLexTestFile, symProc))
                break;
            xmlLexTestFile[16] = i / 10 + '0';
            xmlLexTestFile[17] = ++i % 10 + '0';
        } while (true);
        if (i < sizeofarray(testFunctions)) {
            printf("Lex test %d failed.\n", i);
            return 1;
        }

        if (!(randomResults = generateRandomContents(randomFileName)))
            return 1;

        TurnLogOn();
        if (!LoadXmlFile(nullptr, randomFileName, randTest)) {
            printf("Random lex test failed.\n");
            return 1;
        }

        printf("Lex tests completed successfully, commencing parse tests...\n");

        /* parser tests */
        i = 0;
        while (i < numParseNegTestFiles) {
            TurnLogOn();
            printf("Parse negative test: %s...\n", parseNegTestFile + 7);
            if (LoadXmlFile(nullptr, parseNegTestFile, nullptr))
                break;
            parseNegTestFile[22] = ++i / 10 + '0';
            parseNegTestFile[23] = i % 10 + '0';
        }
        XmlFreeAll();
        if (i < numParseNegTestFiles) {
            printf("Parse negative test %d failed.\n", i);
            return 1;
        }

        printf("Parse negative tests completed successfully.\n");

        if (!LoadXmlFile(&tree, "..\\xml\\parse-test-00.xml", nullptr)) {
            printf("Parse test 00 failed at loading.\n");
            return 1;
        }
        if (!verifyParseTree00(tree)) {
            printf("Parse test 00 failed at parse tree verification.\n");
            return 1;
        }
        XmlFreeAll();

        if (!LoadXmlFile(&tree, "..\\xml\\parse-test-01.xml", nullptr)) {
            printf("Parse test 01 failed at loading.\n");
            return 1;
        }
        if (!verifyParseTree01(tree)) {
            printf("Parse test 01 failed at parse tree verification.\n");
            return 1;
        }
        XmlFreeAll();

        for (i = 0; i < 30; i++) {
            XmlNode *loadedTree;
            XmlNode *generatedTree = generateTestTree(10 + rand() % 16);
            if (!SaveXmlFile(generatedTree, parseRandomTestFile, false)) {
                printf("Failed to generate xml file from tree. (%2)\n", i);
                return 1;
            }
            if (!LoadXmlFile(&loadedTree, parseRandomTestFile, nullptr)) {
                printf("Failed to load generated xml file. (%d)\n", i);
                return 1;
            }
            if (!XmlTreesEqual(generatedTree, loadedTree)) {
                printf("Loaded and generated tree mismatch! (%d)\n", i);
                return 1;
            }
            XmlFreeAll();
        }

    } __except (HandleException(_exception_code(), _exception_info())) {}

    printf("All tests pessed OK! (woot!!!)\n");
    return 0;
}