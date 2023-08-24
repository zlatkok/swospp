#include <stdarg.h>
#include <errno.h>
#include "options.h"
#include "xmlparse.h"
#include "mplayer.h"
#include "mpdirect.h"

byte pl2Keyboard asm("pl2Keyboard");

static XmlNode *m_rootNode;
static XmlNode *m_optionsNode;
static XmlNode *m_lastSectionNode;

static bool m_useBIOSForJoystickInput;
static bool m_calibrateJoysticks = true;
static bool m_runningUnderDosbox;

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

    SaveXmlFile(m_rootNode, "swospp.xml", checkIfModified);
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

/** GetXmlNodeName

    p        -> input buffer
    nameBuff -> buffer that will hold the name
    length   -  size of name buffer

    Read XML node name from input buffer, store it to name buffer and return position
    following the name in the input buffer.
*/
static const char *GetXmlNodeName(const char *p, char *nameBuff, int maxSize, int *length)
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
               n - subnode follows (attach it to the last top level node)
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
                assert_msg(0, "Unknown int type encountered.");
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
        p = GetXmlNodeName(p, nameBuff, sizeof(nameBuff) - 1, &nameLength);
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
        AddXmlNode(m_lastSectionNode, sectionNode);
    } else {
        AddXmlNode(m_optionsNode, sectionNode);
        m_lastSectionNode = sectionNode;
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
    AddXmlNode(m_rootNode = NewEmptyXmlNode("SWOSPP", 6), m_optionsNode = m_lastSectionNode = NewEmptyXmlNode("options", 7));
    RegisterSWOSOptions(RegisterOptions);
    RegisterControlsOptions(RegisterOptions);
#ifndef SENSI_DAYS
    RegisterNetworkOptions(RegisterOptions);
    RegisterUserTactics(RegisterOptions);
#endif
    ReverseChildren(m_rootNode);
    XmlTreeSnapshot(m_rootNode);       /* default options */

    if (LoadXmlFile(&fileRoot, "swospp.xml")) {
        calla_ebp_safe(SaveOptions);    /* save options immediately so that restore won't reset them */
        XmlMergeTrees(m_rootNode, fileRoot);
        XmlTreeSnapshot(m_rootNode);    /* options loaded from the file */
    }

    XmlDeallocateTree(fileRoot);
#ifndef SENSI_DAYS
    if (!ValidateUserMpTactics())
        XmlTreeSnapshot(m_rootNode);    /* additional fixes to default or file options */
#endif
}

/** SetDOSBoxDefaultOptions

    Invoke this when DOSBox has been successfully detected to set default options.
    We will by default use BIOS joystick routines since they work WAY better than
    game port polling, and we'll skip calibration too, seems unnecessary.
*/
void SetDOSBoxDefaultOptions()
{
    m_useBIOSForJoystickInput = true;
    m_calibrateJoysticks = false;
    m_runningUnderDosbox = true;
}

bool GetUseBIOSJoystickRoutineOption()
{
    return m_useBIOSForJoystickInput;
}

bool GetCalibrateJoysticksOption()
{
    return m_calibrateJoysticks;
}

bool DOSBoxDetected()
{
    return m_runningUnderDosbox;
}

/*
    Command line stuff
*/

#ifndef SENSI_DAYS
/** GetCommandLine

    buff -> buffer to receive the command line

    Transfer command line string from PSP to user-supplied buffer. Buffer has to be big enough,
    including space for terminating zero. That means 257 bytes in the worst case, since command
    line length is a byte.
*/
static void GetCommandLine(char *buff)
{
    asm volatile (
        "push es                \n"
        "push ds                \n"

        "mov  ah, 0x62          \n"
        "int  0x21              \n"     // ebx -> PSP

        "mov  eax, ds           \n"
        "mov  es, eax           \n"
        "mov  ds, ebx           \n"

        "mov  esi, 0x80         \n"     // offset 0x80 string length, offset 0x81 string itself
        "movzx ecx, byte ptr [esi]  \n"
        "jcxz .out              \n"

        "inc  esi               \n"
        "rep  movsb             \n"

".out:                          \n"
        "mov  byte ptr es:[edi], 0  \n"
        "pop  ds                \n"
        "pop  es                \n"

        : "+D" (buff)
        :
        : "eax", "ebx", "ecx", "esi", "memory", "cc"
    );
}

static const char *GetBoolValue(const char *p, bool& value)
{
    if (*p == '=' && (p[1] == '1' || p[1] == '0')) {
        value = p[1] - '0';
        p += 2;
    }

    return p;
}

static const char *ParseJoystickOptions(const char *p)
{
    bool value = true;

    if (*p == 'b') {
        p = GetBoolValue(p + 1, value);
        m_useBIOSForJoystickInput = value;
    }

    if (*p == 'c') {
        p = GetBoolValue(p + 1, value);
        m_calibrateJoysticks = value;
    }

    return p;
}
#endif

/** ParseCommandLine

    SWOS++ command line parsing. Switches are case insensitive. Switch parameters
    follow switches immediately, no spaces. If switch parameter is a string, and it
    contains spaces, start and end it with a quote (").
*/
void ParseCommandLine()
{
#ifndef SENSI_DAYS
    char cmdLine[257];
    GetCommandLine(cmdLine);

    const char *end;
    bool isServer = false;

    for (const char *p = cmdLine; *p; p++) {
        if (*p == '/') {
            end = ++p;
            switch (*p++) {
            case 's':
                isServer = true;
                [[fallthrough]];

            case 'c':
                SetDirectGameName(isServer, p, end = GetStringEnd(p));
                break;

            case 't':
                SetTimeout(p, end = GetStringEnd(p));
                break;

            case 'i':
                SetCommFile(p, end = GetStringEnd(p));
                break;

            case 'p':
                SetNickname(p, end = GetStringEnd(p));
                break;

            case 'j':
                end = ParseJoystickOptions(p);
                break;
            }

            p = end + (*p == '"');
        }
    }

    WriteToLog("Command line is: \"%s\"", cmdLine);
    DirectModeOnCommandLineParsingDone();
#endif
}

const char *GetStringEnd(const char *start)
{
    auto end = start;

    if (*end == '"') {
        end++;
        while (*end && *end != '"')
            end++;
    } else {
        while (*end && !isspace(*end))
            end++;
    }

    return end - 1;
}
