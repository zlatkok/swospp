#include <stdarg.h>
#include <errno.h>
#include "swos.h"
#include "util.h"
#include "options.h"
#include "xmlparse.h"
#include "mplayer.h"
#include "dos.h"

byte pl2Keyboard asm("pl2Keyboard");

static XmlNode *rootNode;
static XmlNode *optionsNode;
static XmlNode *lastSectionNode;

extern "C" void RegisterNetworkOptions(RegisterOptionsFunc registerOptions);
extern "C" void RegisterControlsOptions(RegisterOptionsFunc registerOptions);
extern "C" void RegisterUserTactics(RegisterOptionsFunc registerOptions);


/** SaveOptionsIfNeeded

    Save options unless there are no changes.
    Called at the end of program (be it normal or abnormal exit).
*/
void SaveOptionsIfNeeded()
{
    dword file;
    bool checkIfModified = false;

    /* if file is missing create it anyway */
    if ((file = OpenFile(F_READ_ONLY, "swospp.xml") != INVALID_HANDLE)) {
        CloseFile(file);
        checkIfModified = true;
    }

    SaveXmlFile(rootNode, "swospp.xml", checkIfModified);
}


static XmlNodeType getIntType(int length)
{
    switch (length) {
    case 1:
        return XML_CHAR;
    case 2:
        return XML_SHORT;
    case 4:
        return XML_INT;
    default:
        assert_msg(0, "Invalid integer length in options stream");
        return XML_EMPTY;
    }
}


/** getXmlNodeName

    p        -> input buffer
    nameBuff -> buffer that will hold the name
    length   -  size of name buffer

    Read xml node name from input buffer, store it to name buffer and return position
    following the name in the input buffer.
*/
static const char *getXmlNodeName(const char *p, char *nameBuff, int maxSize, int *length)
{
    int spaceLeft = maxSize;
    for (; *p != '\0' && *p != '%' && maxSize; spaceLeft--)
        *nameBuff++ = *p++;
    assert_msg(spaceLeft || *p == '%' || *p == '\0', "Too long xml node name found.");
    nameBuff[-!spaceLeft] = '\0';
    *length = maxSize - spaceLeft;
    return p;
}


/** RegisterOptions

    section -> name of this options section
    desc    -> description of this section
    format  -> string describing types of options given (description below)
    ...     -> variable number of pointer to each option variable

    Format:
    (%[<length>]<type>/?<name>)+

    <length> - integer determining length of the following type, optional
               if * length will be given in parameters
    <type>   - type of the option, can be:
               d - integer
               s - string
               b - binary array
               n - subnode follows
               c - callback to a function that will return pointer to variables
    <name>   - name of the option, it is terminated by next % or end of string
               fields without names are considered fillers, and are ignored
               if * name will be given in parameters

    Whitespace is ignored.
    Register these options as default, and keep the list for subsequent modification, saving and loading.
*/
void __cdecl RegisterOptions(const char *section, int sectionLen, const char *desc, int descLen, const char *format, ...)
{
    const char *p = format;
    int length, result;
    enum XmlNodeType type;
    XmlValue value;
    XmlNode *sectionNode = NewXmlNode(section, sectionLen, XML_EMPTY, 0, false);
    XmlNode *newNode;
    char nameBuff[32];
    int nameLength;
    bool isSubNode = false, gotFunc = false;
    va_list va;

    assert(strlen(section) == sectionLen);
    assert(strlen(desc) == descLen);
    assert(sectionNode);

    va_start(va, format);
    while (*p) {
        memset(&value, 0, sizeof(value));
        skipWhitespace(p);
        assert(*p == '%');
        if (*++p == '*') {  /* skip % and test for variable length */
            length = va_arg(va, int);
            assert(length > 0);
            p++;
        } else {
            const char *old = p;
            length = strtol(p, &p, 0, &result); /* length is optional */
            assert(p == old || result == EZERO);
            if (p == old)
                length = 4; /* default length */
        }
        switch (*p++) {
        case 'd':
            type = getIntType(length);
            switch (type) {
            case XML_CHAR:
                value.charVal = va_arg(va, signed char *);
                break;
            case XML_SHORT:
                value.shortVal = va_arg(va, signed short *);
                break;
            case XML_INT:
                value.intVal = va_arg(va, signed int *);
                break;
            default:
                assert_msg(0, "Uknown int type encountered.");
            }
            break;
        case 's':
            type = XML_STRING;
            value.ptr = va_arg(va, char *);
            break;
        case 'b':
            assert_msg(length > 0, "Binary arrays must specify length.");
            type = XML_ARRAY;
            value.ptr = va_arg(va, char *);
            break;
        case 'n':
            /* this node is subnode of the last */
            isSubNode = true;
            continue;
        case 'c':
            /* node will retrieve pointer to children's values by invoking user supplied function */
            gotFunc = true;
            SetFunc(sectionNode, va_arg(va, XmlValueFunction));
            assert(XmlNodeGetFunc(sectionNode));
            continue;
        default:
            assert_msg(0, "Invalid code in option stream.");
            return;
        }

        /* we have type here, get name */
        p += *p == '/';
        nameBuff[sizeof(nameBuff) - 1] = '\0';
        p = getXmlNodeName(p, nameBuff, sizeof(nameBuff) - 1, &nameLength);
        assert((nameLength != 0) == (nameBuff[0] != '\0'));
        assert(*p == '%' || !*p);
        AddXmlNode(sectionNode, newNode = NewXmlNode(nameBuff, nameLength, type, length, false));
        if (!gotFunc)
            XmlNodeSetValue(newNode, value);
    }
    va_end(va);

    if (desc)
        AddXmlNodeAttribute(sectionNode, "desc", 4, desc, descLen);
    if (isSubNode) {
        AddXmlNode(lastSectionNode, sectionNode);
    } else {
        AddXmlNode(optionsNode, sectionNode);
        lastSectionNode = sectionNode;
    }
}


void RegisterSWOSOptions(RegisterOptionsFunc registerOptions)
{
    registerOptions("SWOS", 4, "Original SWOS options", 21,
        "%2d/gameLength" "%2d/autoReplays" "%2d/menuMusic" "%2d/autoSaveHighlights"
        "%2d/allPlayerTeamsEqual" "%2d/pitchType" "%2d/commentary" "%2d/chairmanScenes",
        &gameLength, &autoReplays, &menuMusic, &autoSaveHighlights,
        &allPlayerTeamsEqual, &pitchType, &commentary, &chairmanScenes);
}


void InitializeOptions()
{
    XmlNode *fileRoot;
    WriteToLog("Loading options...");
    AddXmlNode(rootNode = NewEmptyXmlNode("SWOSPP", 6), optionsNode = lastSectionNode = NewEmptyXmlNode("options", 7));
    RegisterSWOSOptions(RegisterOptions);
    RegisterControlsOptions(RegisterOptions);
    RegisterNetworkOptions(RegisterOptions);
    RegisterUserTactics(RegisterOptions);
    ReverseChildren(rootNode);
    XmlTreeSnapshot(rootNode);      /* default options */

    if (LoadXmlFile(&fileRoot, "swospp.xml")) {
        calla_ebp_safe(SaveOptions);    /* save options immediately so that restore won't reset them */
        XmlMergeTrees(rootNode, fileRoot);
        XmlTreeSnapshot(rootNode);      /* options loaded from the file */
    }

    XmlDeallocateTree(fileRoot);
    if (!ValidateUserMpTactics())
        XmlTreeSnapshot(rootNode);  /* additional fixes to default or file options */
}
