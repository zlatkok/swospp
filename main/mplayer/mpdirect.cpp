/** mpdirect.cpp

    Support for direct multiplayer mode. Depending on command line, redirects SWOS to appropriate
    lobby, in order to allow interaction with front-end. Display menu with animation to the user
    while doing that, since it might take a while.
*/

#include "swos.h"
#include "util.h"
#include "mplayer.h"


extern "C" void SetGameLobbyMenuServerMode(bool);
extern "C" char *GetDirectConnectMenu();
extern "C" char *GetGameLobbyMenu();
extern "C" void JoinRemoteGame(int gameIndex);

static bool directServerMode;
static byte directGameNameLength;
static char directGameName[GAME_NAME_LENGTH + 1];
static char directGameNickname[NICKNAME_LEN + 1];
static char commFile[9];
static word directConnectTimeout = 30 * 70;         /* default timeout if nothing given */
static int directGameIndex = -1;


/** GetDirectGameName

    Return pointer to the direct mode game name. Must always return valid pointer.
*/
const char *GetDirectGameName()
{
    return directGameName;
}


static void SetDirectServerMode(bool isServer, const char *start, const char *end)
{
    assert(end + 1 >= start);
    directServerMode = isServer;
    assert(GAME_NAME_LENGTH < 256);
    start += *start == '"';
    directGameNameLength = strncpy(directGameName, start, min(end - start + 1, GAME_NAME_LENGTH)) - directGameName;
    directGameName[directGameNameLength] = '\0';
}


static void SetNickname(const char *start, const char *end)
{
    assert(end + 1 >= start);
    start += *start == '"';
    *strncpy(directGameNickname, start, min(end - start + 1, NICKNAME_LEN)) = '\0';
}


static void SetCommFile(const char *start, const char *end)
{
    assert(end + 1 >= start);
    *strncpy(commFile, start, min(end - start + 1, (int)sizeof(commFile) - 1)) = '\0';
}


static void SetTimeout(const char *start, const char *end)
{
    int newTimeout = 0;

    while (start <= end && isdigit(*start))
        newTimeout = newTimeout * 10 + *start++ - '0';

    newTimeout *= 70;

    newTimeout = max(5 * 70, newTimeout);
    newTimeout = min(60 * 70, newTimeout);

    directConnectTimeout = newTimeout;
}


const char *GetDirectGameNickname()
{
    return directGameNickname;
}


static const char *GetStringEnd(const char *start)
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


/** ShowErrorAndQuit

    message -> message to show in a menu before quitting

    Display SWOS menu with given message, and "exit" button. When player presses
    exit shut down SWOS cleanly.
*/
extern "C" void ShowErrorAndQuit(const char *message = nullptr)
{
    if (message) {
        A0 = (dword)strupr((char *)message);
        menuFade = 1;
        *(dword *)(continueMenu + 0x36) = (dword)aExit;
        *((char *)ShowMenu + 0x7b) = 0xc3;  /* skip after draw since there is no previous menu */
        calla_ebp_safe(ShowErrorMenu);
    }
    SwitchToPrevVideoMode();
    EndProgram(false);
}


/** ParseCommandLine

    SWOS++ command line parsing. Switches are case insensitive. Switch parameters
    follow switches immediately, no spaces. If switch parameter is a string, and it
    contains spaces, start and end it with a quote (").
*/
static void ParseCommandLine()
{
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
                /* pass through */
            case 'c':
                SetDirectServerMode(isServer, p, end = GetStringEnd(p));
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
            }
            p = end + (*p == '"');
        }
    }

    strupr(directGameName);
    strupr(directGameNickname);

    WriteToLog("Command line is: \"%s\"", cmdLine);

    if (directGameName[0]) {
        WriteToLog("Direct connect mode: %d (%s), gameName: \"%s\"", directGameName[0] != '\0', directServerMode ? "server" : "client", directGameName);
        WriteToLog("Nickname for direct connect: \"%s\", timeout = %d", directGameNickname, directConnectTimeout);
        WriteToLog("Communication file: \"%s\"", commFile);
    }
}


/** MainMenuSelect

    Parse command line looking for direct mode switches and return main menu SWOS will show.
    Called at SWOS startup.
*/
extern "C" const char *MainMenuSelect()
{
    ParseCommandLine();

    if (GetDirectGameName()[0]) {
        auto failureReason = InitMultiplayer();
        if (!failureReason) {
            SetGameLobbyMenuServerMode(directServerMode);
            return directServerMode ? GetGameLobbyMenu() : GetDirectConnectMenu();
        } else
            ShowErrorAndQuit(failureReason);
    }

    /* no direct mode switches, just return main menu and everything will go as usual */
    return SWOS_MainMenu;
}


static word connectingAnimationTimer;

/** SearchForTheDirectConnectGame

    We get called from network module and given all games found so far. Look through
    it and see if one of them is the one we look for; set directGameIndex if we found it.
*/
static void SearchForTheDirectConnectGame(const WaitingToJoinReport *report)
{
    if (directGameIndex < 0) {
        for (int i = 0; i < MAX_GAMES_WAITING; i++) {
            if (report->waitingGames[i] == (char *)-1)
                break;
            if (!strncmp(report->waitingGames[i], directGameName, directGameNameLength)) {
                directGameIndex = i;
                break;
            }
        }
    }
}


static constexpr int GetConnectingEntryIndex()
{
    return 1;
}

static MenuEntry *GetConnectingEntry()
{
    D0 = GetConnectingEntryIndex();
    calla_ebp_safe(CalcMenuEntryAddress);
    return (MenuEntry *)A0;
}


static int connectingStringLength;
static word connectingStartTick;

/** SearchForTheGameInit

    Initialize dialog and animation that will be shown to the player while we are
    looking for the specified direct mode game to connect to.
*/
extern "C" void SearchForTheGameInit()
{
    auto connectingEntry = GetConnectingEntry();
    connectingEntry->disabled = true;
    connectingStringLength = strlen(connectingEntry->u2.string);

    connectingAnimationTimer = connectingStartTick = currentTick;
    EnterWaitingToJoinState(SearchForTheDirectConnectGame, directConnectTimeout);
}


/** UpdateConnectingAnimation

    Game search is under-way, update animation.
*/
static void UpdateConnectingAnimation()
{
    static const word NEXT_FRAME_TICKS = 21;

    auto connectingEntry = GetConnectingEntry();

    auto now = currentTick;
    if (connectingAnimationTimer + NEXT_FRAME_TICKS < now) {
        while (connectingAnimationTimer <= now)
            connectingAnimationTimer += NEXT_FRAME_TICKS;

        static int animationState = 3;
        int currentState = animationState++ % 4;
        int i = 0;
        while (i < currentState)
            connectingEntry->u2.string[connectingStringLength - 3 + i++] = '.';
        connectingEntry->u2.string[connectingStringLength - 3 + i] = '\0';

        A5 = (dword)connectingEntry;
        calla(DrawMenuItem);
    }
}


/** HideSearchDialog

    Hides all menu elements related to game search.
*/
static void HideSearchDialog()
{
    const int numEntriesToHide = 2;
    MenuEntry *entry = GetConnectingEntry();
    for (int i = 0; i < numEntriesToHide; i++, entry++)
        entry->invisible = true;
}


/** UpdateGameSearch

    Called each frame during initial direct connect search for specified game. See how
    we're doing and dispatch to proper handler.
*/
extern "C" void UpdateGameSearch()
{
    if (directGameIndex >= 0) {
        WriteToLog("Direct connect game found! Trying to connect...");
        HideSearchDialog();
        JoinRemoteGame(directGameIndex);
    } else if (currentTick > connectingStartTick + directConnectTimeout)
        ShowErrorAndQuit("Game not found.");
    else
        UpdateConnectingAnimation();
}
