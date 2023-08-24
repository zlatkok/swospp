/** xmlparse.c

    Main focal point of XML parser. Made up of lexer, parser and load/save routines/support.
*/

#include <errno.h>
#include "xmlparse.h"
#include "xmltree.h"
#include "qalloc.h"
#include "bfile.h"

struct XmlNodeListElem {
    XmlNode *node;
    struct XmlNodeListElem *next;
};

struct XmlContentListElem {
    char *content;
    int size;
    int totalSize;
    struct XmlContentListElem *next;
};

enum XmlParserState {
    STATE_START,
    STATE_TAG_OPENED,
    STATE_INSIDE_TAG,
    STATE_GET_ATTR_EQ,
    STATE_GET_ATTR_VAL,
    STATE_GET_POSSIBLE_CONTENT,
    STATE_END_TAG_OPENED,
    STATE_EXPECT_END_TAG_CLOSE,
};

/* parser internal variables... obviously, we're not thread-safe :P */
static int lineNo;
static int linesSkipped;
static const char *xmlFile;
static bool inTag;
static void *xmlHeap;
static XmlParserState parserState;
static char *attrName;
static int attrNameLen;
static XmlNodeListElem *nodeStack;
static XmlNode *xmlRoot;

static void initializeParser(const char *fileName, void *heap, int heapSize)
{
    lineNo = 1;
    linesSkipped = 0;
    xmlFile = strrchr(fileName, '\\');
    xmlFile = xmlFile ? xmlFile + 1 : fileName;
    inTag = false;
    qHeapInit(heap, heapSize);
    xmlHeap = heap;
    parserState = STATE_START;
    nodeStack = nullptr;
    attrName = nullptr;
    attrNameLen = 0;
    xmlRoot = nullptr;
}

#ifdef DEBUG
static void report(const char *text)
{
    WriteToLog("%s(%d): %s", xmlFile, lineNo, text);
}
#else
#define report(text) ((void)0)
#endif

static const char *SymToStr(XmlSymbol sym)
{
    static_assert(XML_SYM_MAX == 7, "Xml symbols changed.");
    switch (sym) {
    case XML_SYM_TAG_OPEN:
        return "<";
    case XML_SYM_TAG_OPEN_END_TAG:
        return "</";
    case XML_SYM_TAG_CLOSE:
        return ">";
    case XML_SYM_TAG_CLOSE_SOLO_TAG:
        return "/>";
    case XML_SYM_EQ:
        return "=";
    case XML_SYM_TEXT:
        return "<text>";
    case XML_SYM_EOF:
        return "end of file";
    case XML_SYM_MAX:
        return "invalid symbol";
    default:
        assert_msg(0, "who was messing with xml symbols enum?");
        return "";
    }
}

static void *XmlAlloc(int size)
{
    void *mem = qHeapAlloc(xmlHeap, size);
    assert(xmlHeap && size > 0);

    if (!mem)
        report("error: Out of memory.");

    return mem;
}

static char *XmlStrdup(char *data, int len)
{
    char *copy = (char *)XmlAlloc(len + 1);
    assert(len > 0);

    if (copy) {
        memcpy(copy, data, len);
        copy[len] = '\0';
        return copy;
    }

    return nullptr;
}

static void XmlFree(void *ptr)
{
    qHeapFree(xmlHeap, ptr);
}

static bool PushNode(XmlNode *node)
{
    XmlNodeListElem *elem = (XmlNodeListElem *)XmlAlloc(sizeof(XmlNodeListElem));
    assert(xmlHeap);

    if (!elem)
        return false;

    elem->node = node;
    elem->next = nodeStack;

    if (nodeStack)
        AddXmlNode(nodeStack->node, node);

    nodeStack = elem;
    return true;
}

static XmlNode *PopNode()
{
    XmlNodeListElem *elem = nodeStack;
    assert(xmlHeap && nodeStack);
    nodeStack = nodeStack->next;
    return elem->node;
}

static XmlNode *TopNode()
{
    assert(xmlHeap && nodeStack);
    return nodeStack->node;
}

static XmlNode *PeekTopNode()
{
    assert(xmlHeap);
    return nodeStack ? nodeStack->node : nullptr;
}

/** GetXmlEscape

    file -> buffered XML file to read from

    & was already encountered, by using state matching determine if we have XML escape, one of:

    &lt;    <   less than
    &gt;    >   greater than
    &amp;   &   ampersand
    &apos;  '   apostrophe
    &quot;  "   quotation mark

    Return character replacement or 0 if not escape sequence.
*/
static char GetXmlEscape(BFile *file)
{
    int readChars[4];       /* hold all characters encountered so far */
    int count = 1;          /* keep count of read characters */
    readChars[0] = GetCharBFile(file);

    assert(file);
    /* &lt; */
    if (readChars[0] == 'l') {
        readChars[count++] = GetCharBFile(file);
        if (readChars[1] == 't') {
            readChars[count++] = GetCharBFile(file);
            if (readChars[2] == ';')
                return '<';
        }
    /* &gt; */
    } else if (readChars[0] == 'g') {
        readChars[count++] = GetCharBFile(file);
        if (readChars[1] == 't') {
            readChars[count++] = GetCharBFile(file);
            if (readChars[2] == ';')
                return '>';
        }
    /* &amp; and &apos; */
    } else if (readChars[0] == 'a') {
        readChars[count++] = GetCharBFile(file);
        if (readChars[1] == 'm') {
            readChars[count++] = GetCharBFile(file);
            if (readChars[2] == 'p') {
                readChars[count++] = GetCharBFile(file);
                if (readChars[3] == ';')
                    return '&';
            }
        } else if (readChars[1] == 'p') {
            readChars[count++] = GetCharBFile(file);
            if (readChars[2] == 'o') {
                readChars[count++] = GetCharBFile(file);
                if (readChars[3] == 's') {
                    readChars[count++] = GetCharBFile(file);
                    if (readChars[4] == ';')
                        return '\'';
                }
            }
        }
    /* &quot; */
    } else if (readChars[0] == 'q') {
        readChars[count++] = GetCharBFile(file);
        if (readChars[1] == 'u') {
            readChars[count++] = GetCharBFile(file);
            if (readChars[2] == 'o') {
                readChars[count++] = GetCharBFile(file);
                if (readChars[3] == 't') {
                    readChars[count++] = GetCharBFile(file);
                    if (readChars[4] == ';')
                        return '"';
                }
            }
        }
    }

    /* no match, return all read characters to stream */
    while (count--)
        UngetCharBFile(file, readChars[count]);

    return 0;
}

/** GetStringEscape

    file -> buffered XML file to read from

    Slash inside a string was encountered, resolve and return any escaped characters.
    Return -1 for invalid escape sequence.
*/
static int GetStringEscape(BFile *file)
{
    int readChars[3];
    size_t count = 1;

    assert(file);
    readChars[0] = GetCharBFile(file);

    if (readChars[0] == '\\')
        return '\\';
    else if (readChars[0] == 'n')
        return '\n';
    else if (readChars[0] == 'r')
        return '\r';
    else if (readChars[0] == 't')
        return '\t';
    else if (readChars[0] == 'b')
        return '\b';
    else if (readChars[0] == '"')
        return '"';
    else if (readChars[0] == '\'')
        return '\'';
    else if (readChars[0] == 'x') {
        uint32_t val = 0;
        int c;
        int hexCount = 0;

        while (c = GetCharBFile(file), isxdigit(c) && hexCount++ < 2) {
            if (count < sizeofarray(readChars)) {
                readChars[count++] = c;
                val = (val << 4) | (c > '9' ? (c | 0x20) - 'a' + 10 : c - '0');
            }
        }

        UngetCharBFile(file, c);

        if (!hexCount)
            report("\\x used with no following hex digits.");

        if (hexCount > 0)
            return val & 0xff;
    } else if (isdigit(readChars[0])) {
        int c;
        int digitCount = 1;
        int val = readChars[0] - '0';
        while (c = GetCharBFile(file), isdigit(c)) {
            if (count < sizeofarray(readChars)) {
                /* screw octal, who the hell needs that */
                readChars[count++] = c;
                val = val * 10 + c - '0';
            }
            digitCount++;
        }

        UngetCharBFile(file, c);

        if (digitCount > 3 || val > 255)
            report("Numeric escape sequence out of range.");
        return val & 0xff;
    }
    /* no match, return all read characters to stream */
    assert((int)count >= 0 && count < sizeofarray(readChars));

    while (count--)
        UngetCharBFile(file, readChars[count]);

    return -1;
}

/** SkipXmlComment

    file -> buffered XML file to read from

    Start XML comment symbol has already been found, now skip everything until end of comment symbol.
*/
static void SkipXmlComment(BFile *file)
{
    int c;
    int count;
    char readChars[2];
    assert(file);

    while ((c = GetCharBFile(file)) >= 0) {
        count = 0;
        if (c == '-') {
            readChars[count++] = GetCharBFile(file);
            if (readChars[0] == '-') {
                readChars[count++] = GetCharBFile(file);
                if (readChars[1] == '>')
                    return;
            }
        } else if (c == '\n')
            lineNo++;
        while (count--)
            UngetCharBFile(file, readChars[count]);
    }

    if (c < 0)
        report("Unterminated comment found.");

    UngetCharBFile(file, c);
}

/** GetText

    file    -> buffered XML file to read from
    inTag   -  true if we are inside XML tag, break text into tokens then
    buf     -> buffer to receive collected text
    bufSize -> maximum size of text buffer

    Terminate buffer with zero and return number of characters written to buffer (not counting ending zero).
    Any text blocks interspersed with strings will be merged and returned as a single block.
*/
static int GetText(BFile *file, bool inTag, char *buf, int bufSize)
{
    int c;
    int inQuote = 0;
    char *p = buf, *stringEnd = buf;

    assert(file);
    assert(bufSize >= 0);
    assert(buf);

    /* get rid of initial whitespace */
    while (isspace(PeekCharBFile(file)))
        lineNo += GetCharBFile(file) == '\n';

    while ((c = GetCharBFile(file)) >= 0) {
        if (c == '&') {
            char escaped = GetXmlEscape(file);
            if (escaped)
                c = escaped;
        } else if (c == '"') {
            /* get rid of whitespaces before and after strings */
            if (!(inQuote ^= 1)) {
                stringEnd = max(p - 1, buf);
                if (!inTag)
                    while (isspace(PeekCharBFile(file)))
                        lineNo += GetCharBFile(file) == '\n';
            } else
                while (p - 1 > stringEnd && isspace(p[-1]))
                    lineNo -= *--p == '\n';
            continue;
        } else if (inQuote) {
            if (c == '\\') {
                int escaped = GetStringEscape(file);
                if (escaped >= 0)
                    c = escaped;
            }
        } else if (c == '<') {
            /* we can't warn about nested tags here, as it might only be a comment */
            UngetCharBFile(file, '<');
            break;
        } else if (c == '/') {
            if (PeekCharBFile(file) == '>') {
                UngetCharBFile(file, '/');
                break;
            }
        } else if (c == '>') {
            UngetCharBFile(file, '>');
            break;
        } else if (inTag) {
            if (c == '=') {
                UngetCharBFile(file, '=');
                break;
            } else if (isspace(c)) {
                while (isspace(PeekCharBFile(file)))
                    lineNo += GetCharBFile(file) == '\n';
                break;
            }
        }

        if (bufSize > 0) {
            *p++ = c;
            bufSize--;
            lineNo += c == '\n';
        }
    }
    if (inQuote) {
        report("Unterminated string found.");
    } else {
        /* remove trailing whitespace if not quoted */
        if (p > buf) {
            int currentLineNo = lineNo;

            while (isspace(p[-1]) && p > stringEnd + 1)
                lineNo -= *--p == '\n';

            /* we have to report line number for this symbol correctly, but we also have to
               maintain correct line number for the symbol that follows */
            linesSkipped = currentLineNo - lineNo;
            assert(linesSkipped >= 0);
        }
    }
    assert(p >= buf);
    if (bufSize > 0)
        *p = '\0';

    else if (p > buf)
        *--p = '\0';

    return p - buf;
}

/** GetSymbol

    file    -> buffered file to read from
    buf     -> buffer to store found text to
    bufSize -> the size of given buffer on entry,
               number of characters written on exit

    Main lexer routine. Return symbol found, along with text, if any. Buffer will always be
    null-terminated if it's size is at least one. Size returned will not include ending zero.
*/
static XmlSymbol GetSymbol(BFile *file, char *buf, int *bufSize)
{
    int numCharsMatched;
    int matchedChars[3];

    int c = GetCharBFile(file);
    lineNo += linesSkipped;
    linesSkipped = 0;

    switch (c) {
    case '<':
        /* this could be open tag, open ending tag or an xml comment */
        numCharsMatched = 1;
        matchedChars[0] = GetCharBFile(file);
        if (matchedChars[0] == '/') {
            inTag = true;
            return XML_SYM_TAG_OPEN_END_TAG;
        } else if (matchedChars[0] == '!') {
            matchedChars[numCharsMatched++] = GetCharBFile(file);
            if (matchedChars[1] == '-') {
                matchedChars[numCharsMatched++] = GetCharBFile(file);
                if (matchedChars[2] == '-') {
                    SkipXmlComment(file);
                    return GetSymbol(file, buf, bufSize);
                }
            }
        }
        while (numCharsMatched--)
            UngetCharBFile(file, matchedChars[numCharsMatched]);
        inTag = true;
        return XML_SYM_TAG_OPEN;

    case '>':
        inTag = false;
        return XML_SYM_TAG_CLOSE;

    case '/':
        if (PeekCharBFile(file) == '>') {
            GetCharBFile(file);
            inTag = false;
            return XML_SYM_TAG_CLOSE_SOLO_TAG;
        }
        break;

    case '=':
        if (inTag)
            return XML_SYM_EQ;
        break;

    case -1:
        return XML_SYM_EOF;
    }

    UngetCharBFile(file, c);
    /* don't return 0-sized text */
    return (*bufSize = GetText(file, inTag, buf, *bufSize)) ? XML_SYM_TEXT : GetSymbol(file, buf, bufSize);
}

static bool Expect(XmlSymbol expected, XmlSymbol given)
{
    if (expected != given) {
        WriteToLog("%s(%d): error: Expected '%s', got '%s' instead.",
            xmlFile, lineNo, SymToStr(expected), SymToStr(given));
        return false;
    }

    return true;
}

static bool FixXmlNodeContent(XmlNode *node)
{
    XmlContentListElem *elem;
    char *contents;
    int currentOffset, totalSize;
    assert(node);

    if (node->type != XML_STRING && node->type != XML_ARRAY)
        return true;

    if (node->type == XML_STRING && !node->value.ptr) {
        node->value.ptr = (char *)XmlAlloc(1);
        node->value.ptr[0] = '\0';
        node->length = 1;
        return true;
    }

    assert(node->value.ptr);
    totalSize = ((XmlContentListElem *)node->value.ptr)->totalSize;
    assert(totalSize > 0);
    contents = (char *)XmlAlloc(totalSize + (node->type == XML_STRING));
    assert(contents);

    if (!contents)
        return false;

    elem = (XmlContentListElem *)node->value.ptr;
    currentOffset = totalSize;

    if (node->type == XML_STRING)
        contents[currentOffset] = '\0';

    while (elem) {
        currentOffset -= elem->size;
        memcpy(contents + currentOffset, elem->content, elem->size);
        elem = elem->next;
    }

    assert(currentOffset == 0);
    node->value.ptr = contents;
    node->length = totalSize + (node->type == XML_STRING);
    return true;
}

static bool AddXmlPartialContent(XmlNode *node, char *content, int size)
{
    int value, error, length;
    const char *endPtr;
    bool outOfRange = false;
    XmlNodeType type;

    assert(node && content && size >= 0);
    type = XmlNodeGetType(node);

    if (type == XML_EMPTY) {
        report("error: Empty node can't have content.");
        return false;
    } else if (type != XML_STRING && type != XML_ARRAY) {
        if (XmlNodeHasContent(node)) {
            report("error: Only strings and arrays are allowed to be splitted.");
            return false;
        }
        switch (type) {
        case XML_CHAR:
        case XML_SHORT:
        case XML_INT:
            value = strtol(content, &endPtr, 10, &error);
            if (error == ERANGE)
                outOfRange = true;
            else if (error == EZERO && endPtr == content + size) {
                if (type == XML_CHAR && (value < -128 || value > 127))
                    outOfRange = true;
                else if (type == XML_SHORT && (value < -32768 || value > 32767))
                    outOfRange = true;
            }
            if (outOfRange) {
                report("Numerical value out of specified range.");
            } else if (error != EZERO || endPtr != content + size) {
                report("error: Invalid numerical constant.");
                return false;
            }
            length = 1;
            if (type == XML_SHORT)
                length = 2;
            else if (type == XML_INT)
                length = 4;
            return AddXmlContent(node, type, &value, length);
        default:
            return AddXmlContent(node, type, content, size);
        }
    } else {
        XmlContentListElem *lastElem = (XmlContentListElem *)node->value.ptr;
        XmlContentListElem *elem = (XmlContentListElem *)XmlAlloc(sizeof(XmlContentListElem));

        if (!elem)
            return false;

        elem->content = XmlStrdup(content, size);
        elem->size = elem->totalSize = size;

        if (lastElem)
            elem->totalSize += lastElem->totalSize;

        elem->next = lastElem;
        node->value.ptr = (char *)elem;
    }

    return true;
}

/** ProcessSymbol

    sym      - symbol found
    buf     -> buffer containing symbol data; always zero-terminated if size is at least 1
    bufSize  - size of given buffer

    Parsing work horse. Driven by state machine. Goes to next state based on current state and input.
*/
static bool ProcessSymbol(XmlSymbol sym, char *buf, int bufSize)
{
    XmlNode *newNode;
    switch (parserState) {
    case STATE_START:
        if (sym == XML_SYM_EOF) {
            if (!xmlRoot) {
                report("error: Root element missing.");
                return false;
            }
            return true;
        }

        if (!PeekTopNode() && !Expect(XML_SYM_TAG_OPEN, sym))
            return false;

        parserState = STATE_TAG_OPENED;
        return true;

    case STATE_TAG_OPENED:
        if (!Expect(XML_SYM_TEXT, sym))
            return false;
        assert(bufSize);
        parserState = STATE_INSIDE_TAG;

        /* new node opened, it is root if node stack is empty */
        newNode = NewEmptyXmlNode(buf, bufSize);
        if (!PeekTopNode()) {
            if (xmlRoot) {
                report("error: More than one root element found.");
                return false;
            }
            xmlRoot = newNode;
        }
        return PushNode(newNode);

    case STATE_INSIDE_TAG:
        if (sym == XML_SYM_TAG_CLOSE || sym == XML_SYM_TAG_CLOSE_SOLO_TAG) {
            assert(TopNode());
            if (sym == XML_SYM_TAG_CLOSE_SOLO_TAG) {
                if (!FixXmlNodeContent(TopNode()))
                    return false;
                PopNode();
            }
            parserState = PeekTopNode() ? STATE_GET_POSSIBLE_CONTENT : STATE_START;
            return true;
        } else if (sym == XML_SYM_TEXT) {
            XmlFree(attrName);

            if (!(attrName = XmlStrdup(buf, bufSize)))
                return false;

            attrNameLen = bufSize;
            parserState = STATE_GET_ATTR_EQ;
            return true;
        } else {
            char error[128];
            sprintf(error, "error: Unexpected symbol '%s' inside a tag.", SymToStr(sym));
            report(error);
            return false;
        }
        /* assume fall-through */

    case STATE_GET_ATTR_EQ:
        if (!Expect(XML_SYM_EQ, sym))
            return false;
        parserState = STATE_GET_ATTR_VAL;
        return true;

    case STATE_GET_ATTR_VAL:
        if (!(Expect(XML_SYM_TEXT, sym)))
            return false;

        parserState = STATE_INSIDE_TAG;
        assert(attrName && attrNameLen);
        return AddXmlNodeAttribute(TopNode(), attrName, attrNameLen, buf, bufSize);

    case STATE_GET_POSSIBLE_CONTENT:
        if (sym == XML_SYM_TEXT) {
            /* remain in this state */
            return AddXmlPartialContent(TopNode(), buf, bufSize);
        } else if (sym == XML_SYM_TAG_OPEN) {
            parserState = STATE_TAG_OPENED;
            return true;
        } else if (sym == XML_SYM_TAG_OPEN_END_TAG) {
            parserState = STATE_END_TAG_OPENED;
            return true;
        } else {
            char error[128];
            sprintf(error, "error: Unexpected symbol '%s' found inside tag content.", SymToStr(sym));
            report(error);
            return false;
        }
        /* assume fall-through */

    case STATE_END_TAG_OPENED:
        if (!Expect(XML_SYM_TEXT, sym))
            return false;

        if (strncmp(TopNode()->name, buf, bufSize)) {
            report("error: Closing tag doesn't match opening.");
            return false;
        }

        parserState = STATE_EXPECT_END_TAG_CLOSE;
        return true;

    case STATE_EXPECT_END_TAG_CLOSE:
        if (!Expect(XML_SYM_TAG_CLOSE, sym))
            return false;

        if (!FixXmlNodeContent(TopNode()))
            return false;

        PopNode();
        /* go back to start state if we're not inside any tag */
        parserState = PeekTopNode() ? STATE_GET_POSSIBLE_CONTENT : STATE_START;
        return true;

    default:
        assert_msg(0, "Unhandled xml parser state encountered.");
    }

    return false;
}

bool LoadXmlFile(XmlNode **root, const char *fileName, XmlSymbolProcessingFunction symProc)
{
    XmlSymbol sym;
    char xmlBuf[8 * 1024];
    int bufSize = sizeof(xmlBuf);
    char miniHeap[32 * 1024];
    bool success = true;
    BFile *file;

    if (root)
        *root = nullptr;

    if (!(file = OpenBFile(F_READ_ONLY, fileName)))
        return false;

    initializeParser(fileName, miniHeap, sizeof(miniHeap));
    auto oldSplitSize = qSetMinSplitSize(16);

    if (!symProc)
        symProc = ProcessSymbol;

    while ((sym = GetSymbol(file, xmlBuf, &bufSize)) != XML_SYM_EOF) {
        //WriteToLog("state: %d symbol: %s '%s'", parserState, SymToStr(sym), sym == XML_SYM_TEXT ? xmlBuf : "");
        if (!symProc(sym, xmlBuf, bufSize)) {
            qSetMinSplitSize(oldSplitSize);
            CloseBFile(file);
            return false;
        }
        bufSize = sizeof(xmlBuf);
    }

    /* notify callback about EOF too */
    success = symProc(sym, xmlBuf, bufSize);

    CloseBFile(file);
    qSetMinSplitSize(oldSplitSize);

    if (root) {
        ReverseChildren(xmlRoot);
        *root = xmlRoot;
    }

    return success;
}

static bool NeedsQuotes(const char *str, size_t len)
{
    assert(str);
    while (len--) {
        if (*str < 32 || *str > 126)
            return true;

        switch (*str) {
        case '"':
        case '=':
        case '<':
        case '>':
        case '/':
        case '\\':
        case ' ':
            return true;
        }

        str++;
    }
    return false;
}

/* saves a lot of typing :P */
#define OUT_CHAR(c)         { if (!PutCharBFile(file, c)) return false; }
#define OUT_STR(str, len)   { if (!WriteStringToFile(file, str, len, indent, true)) return false; }
#define INDENT(len)         { int i; for (i = 0; i < (len); i++) OUT_CHAR(' '); }

static bool WriteStringToFile(BFile *file, const char *string, size_t stringLen, int indent, bool zeroTerminated)
{
    assert(file && string);
    if (NeedsQuotes(string, stringLen)) {
        int lineChars = indent + 1 + 4;
        OUT_CHAR('"');

        while (stringLen-- && (!zeroTerminated || *string)) {
            if (*string < 32 || *string > 126) {
                int hiByte = (*string & 0xf0) >> 4;
                int loByte = *string & 0x0f;
                OUT_CHAR('\\');
                OUT_CHAR('x');
                OUT_CHAR(hiByte + (hiByte > 9 ? 'a' - 10 : '0'));
                OUT_CHAR(loByte + (loByte > 9 ? 'a' - 10 : '0'));
                lineChars += 4;
            } else if (*string == '"') {
                OUT_CHAR('\\');
                OUT_CHAR('"');
                lineChars += 2;
            } else if (*string == '\\') {
                OUT_CHAR('\\');
                OUT_CHAR('\\');
                lineChars += 4;
            } else if (*string == '\r') {
                OUT_CHAR('\\');
                OUT_CHAR('r');
                lineChars += 4;
            } else if (*string == '\n') {
                OUT_CHAR('\\');
                OUT_CHAR('n');
                lineChars += 4;
            } else if (*string == '\t') {
                OUT_CHAR('\\');
                OUT_CHAR('t');
                lineChars += 4;
            } else {
                OUT_CHAR(*string);
                lineChars++;
            }
            string++;
            if (lineChars > 99 && stringLen) {
                OUT_CHAR('"');
                OUT_CHAR('\n');
                INDENT(indent + 4);
                OUT_CHAR('"');
                lineChars = indent + 1 + 4;
            }
        }

        OUT_CHAR('"');
        return true;
    } else {
        if (stringLen > 100) {
            while (stringLen > 0) {
                INDENT(indent);
                OUT_CHAR('"');
                if (WriteBFile(file, string, 100) != 100)
                    return false;
                OUT_CHAR('"');
                OUT_CHAR('\n');
                stringLen -= 100;
            }
            return true;
        } else
            return WriteBFile(file, string, stringLen) == (int)stringLen;
    }
}

static bool WriteNodeContent(BFile *file, const XmlNode *node, int indent)
{
    int val = 0;
    char *numStr;
    assert(file && node);

    switch (XmlNodeGetType(node)) {
    case XML_CHAR:
        val = *node->value.charVal;
        break;

    case XML_SHORT:
        val = *node->value.shortVal;
        break;

    case XML_INT:
        val = *node->value.intVal;
        break;

    case XML_STRING:
    case XML_ARRAY:
        return WriteStringToFile(file, node->value.ptr, node->length - XmlNodeIsString(node),
            indent, XmlNodeIsString(node));

    case XML_EMPTY:
    case XML_FUNC:  // FIXME
        return true;

    default:
        assert_msg(0, "Don't know how to write this content type!");
    }

    numStr = int2str(val);
    return WriteBFile(file, numStr, strlen(numStr));
}

static bool WriteTreeToFile(BFile *file, const XmlNode *root, int indent)
{
    bool soloNode = false;
    bool isNumericNode;
    static XmlAttributeInfo attributes[64];
    size_t numAttributes;

    if (!root || XmlNodeIsAnonymous(root))
        return true;

    do {
        if (XmlNodeIsAnonymous(root)) {
            root = XmlNodeGetNextSibling(root);
            continue;
        }

        isNumericNode = XmlNodeIsNumeric(root);

        /* open tag and write name */
        INDENT(indent);
        OUT_CHAR('<');
        OUT_STR(XmlNodeGetName(root), XmlNodeGetNameLength(root));

        numAttributes = GetXmlNodeNumAttributes(root);
        /* make sure to write type attribute even if it's not explicitly added, and in 1st spot */
        if (!XmlNodeIsEmpty(root) && !XmlNodeIsFunc(root) && !GetXmlNodeAttribute(root, "type", 4, nullptr)) {
            size_t typeNameLen;
            const char *typeName = XmlNodeTypeToString(XmlNodeGetType(root), &typeNameLen);
            OUT_CHAR(' ');
            OUT_STR("type", 4);
            OUT_CHAR('=');
            OUT_CHAR('"');
            OUT_STR(typeName, typeNameLen);
            OUT_CHAR('"');
        }

        /* write other attributes, if present */
        assert(numAttributes <= sizeofarray(attributes));
        GetXmlNodeAttributes(root, attributes);

        for (size_t i = 0; i < numAttributes; i++) {
            OUT_CHAR(' ');
            OUT_STR(attributes[i].name, attributes[i].nameLength);
            OUT_CHAR('=');
            OUT_STR(attributes[i].value, attributes[i].valueLength);
        }

        if (XmlNodeIsLeaf(root) && !XmlNodeHasContent(root)) {
            soloNode = true;
            OUT_CHAR(' ');
            OUT_CHAR('/');
        } else
            soloNode = false;

        OUT_CHAR('>');

        /* tag closed, write content, if present */
        if (XmlNodeHasContent(root)) {
            if (!isNumericNode) {
                OUT_CHAR('\n');
                INDENT(indent + 4);
            }
            if (!WriteNodeContent(file, root, indent))
                return false;

            if (!isNumericNode)
                OUT_CHAR('\n');
        } else
            OUT_CHAR('\n');

        /* handle custom data nodes */
        if (XmlNodeIsFunc(root))
            RefreshFuncData(root);

        /* write subtree if present */
        if (!XmlNodeIsLeaf(root) && !WriteTreeToFile(file, root->children, indent + (soloNode ? 0 : 4)))
            return false;

        if (!soloNode) {
            if (!isNumericNode)
                INDENT(indent);
            OUT_CHAR('<');
            OUT_CHAR('/');
            OUT_STR(XmlNodeGetName(root), XmlNodeGetNameLength(root));
            OUT_CHAR('>');
            OUT_CHAR('\n');
        }
        root = XmlNodeGetNextSibling(root);
    } while (root);

    return true;
}

bool SaveXmlFile(XmlNode *root, const char *fileName, bool checkIfNeeded)
{
    BFile *file;
    bool result;

    if (!root)
        return false;

    if (checkIfNeeded && XmlTreeUnmodified(root)) {
        WriteToLog("%s not modified, returning.", fileName);
        return true;
    }

    if (!(file = CreateBFile(ATTR_NONE, fileName))) {
        WriteToLog("Failed to create %s.", fileName);
        return false;
    }

    /* mark it as unmodified if we saved it successfully */
    if ((result = WriteTreeToFile(file, root, 0)))
        XmlTreeSnapshot(root);

    if (result)
        WriteToLog("%s written successfully.", fileName);

    CloseBFile(file);
    return result;
}
