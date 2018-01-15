/** mpdirect.cpp

    Support for direct multiplayer mode. Depending on command line, redirects SWOS to appropriate
    lobby, in order to allow interaction with front-end. Display menu with animation to the user
    while doing that, since it might take a while.
*/

#include "mpdirect.h"
#include "mplayer.h"
#include "mplobby.h"

/* from .asm files */
extern "C" void SetGameLobbyMenuServerMode(bool);
extern "C" char *GetDirectConnectMenu();
extern "C" char *GetGameLobbyMenu();
extern "C" void JoinRemoteGame(int gameIndex);

static bool m_directServerMode;
static byte m_directGameNameLength;
static char m_directGameName[GAME_NAME_LENGTH + 1];
static char m_directGameNickname[NICKNAME_LEN + 1];
static char m_commFile[9];
static word m_directConnectTimeout = 30 * 70;       /* default timeout if nothing given */
static int m_directGameIndex = -1;

/** GetDirectGameName

    Return pointer to the direct mode game name. Must always return a valid pointer.
    If this string is non-null, then we are in direct connect mode.
*/
const char *GetDirectGameName()
{
    return m_directGameName;
}

void SetDirectGameName(bool isServer, const char *start, const char *end)
{
    assert(end + 1 >= start);
    m_directServerMode = isServer;
    assert(GAME_NAME_LENGTH < 256);
    start += *start == '"';
    m_directGameNameLength = strncpy(m_directGameName, start, min(end - start + 1, GAME_NAME_LENGTH)) - m_directGameName;
    m_directGameName[m_directGameNameLength] = '\0';
}

void SetNickname(const char *start, const char *end)
{
    assert(end + 1 >= start);
    start += *start == '"';
    *strncpy(m_directGameNickname, start, min(end - start + 1, NICKNAME_LEN)) = '\0';
}

void SetCommFile(const char *start, const char *end)
{
    assert(end + 1 >= start);
    *strncpy(m_commFile, start, min(end - start + 1, (int)sizeof(m_commFile) - 1)) = '\0';
}

void SetTimeout(const char *start, const char *end)
{
    int newTimeout = 0;

    while (start <= end && isdigit(*start))
        newTimeout = newTimeout * 10 + *start++ - '0';

    newTimeout *= 70;

    newTimeout = max(5 * 70, newTimeout);
    newTimeout = min(60 * 70, newTimeout);

    m_directConnectTimeout = newTimeout;
}

const char *GetDirectGameNickname()
{
    return m_directGameNickname;
}

void DirectModeOnCommandLineParsingDone()
{
    strupr(m_directGameName);
    strupr(m_directGameNickname);

    if (m_directGameName[0]) {
        WriteToLog("Direct connect mode: %d (%s), gameName: \"%s\"", m_directGameName[0] != '\0',
            m_directServerMode ? "server" : "client", m_directGameName);
        WriteToLog("Nickname for direct connect: \"%s\", timeout = %d", m_directGameNickname, m_directConnectTimeout);
        WriteToLog("Communication file: \"%s\"", m_commFile);

        /* skip intro animation and jump right into game lobby/game search menu */
        PatchByte(Initialization, 0x19, 1);
    }
}

/** ShowErrorAndQuit

    message -> message to show in a menu before quitting

    Display SWOS menu with given message, and "exit" button. When player presses
    exit shut down SWOS cleanly.
*/
extern "C" void ShowErrorAndQuit(const char *message /* = nullptr*/)
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

/** MainMenuSelect

    Parse command line looking for direct mode switches and return main menu SWOS will show.
    Called at SWOS startup.
*/
extern "C" const char *MainMenuSelect()
{
    if (GetDirectGameName()[0]) {
        auto failureReason = InitMultiplayer();
        if (!failureReason) {
            SetGameLobbyMenuServerMode(m_directServerMode);
            return m_directServerMode ? GetGameLobbyMenu() : GetDirectConnectMenu();
        } else
            ShowErrorAndQuit(failureReason);
    }

    /* no direct mode switches, just return main menu and everything will go as usual */
#ifdef SWOS_16_17
    extern char newMainMenu[] asm ("newMainMenu");
    return newMainMenu;
#else
    return SWOS_MainMenu;
#endif
}

static word m_connectingAnimationTimer;

/** SearchForTheDirectConnectGame

    We get called from network module and given all the games found so far. Look through
    it and see if one of them is the one we look for; set m_directGameIndex if we found it.
*/
static void SearchForTheDirectConnectGame(const WaitingToJoinReport *report)
{
    if (m_directGameIndex < 0) {
        for (int i = 0; i < MAX_GAMES_WAITING; i++) {
            if (report->waitingGames[i] == (char *)-1)
                break;
            if (!strncmp(report->waitingGames[i], m_directGameName, m_directGameNameLength)) {
                m_directGameIndex = i;
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

static int m_connectingStringLength;
static word m_connectingStartTick;

/** SearchForTheGameInit

    Initialize dialog and animation that will be shown to the player while we are
    looking for the specified direct mode game to connect to.
*/
extern "C" void SearchForTheGameInit()
{
    auto connectingEntry = GetConnectingEntry();
    connectingEntry->disabled = true;
    m_connectingStringLength = strlen(connectingEntry->u2.string);
    m_connectingAnimationTimer = m_connectingStartTick = g_currentTick;
    EnterWaitingToJoinState(SearchForTheDirectConnectGame, m_directConnectTimeout);
}

/** UpdateConnectingAnimation

    Game search is underway, update animation.
*/
static void UpdateConnectingAnimation()
{
    const word kNextFrameTicks = 21;
    auto connectingEntry = GetConnectingEntry();

    auto now = g_currentTick;
    if (m_connectingAnimationTimer + kNextFrameTicks < now) {
        while (m_connectingAnimationTimer <= now)
            m_connectingAnimationTimer += kNextFrameTicks;

        static int animationState = 3;
        int currentState = animationState++ % 4;
        int i = 0;

        while (i < currentState)
            connectingEntry->u2.string[m_connectingStringLength - 3 + i++] = '.';

        connectingEntry->u2.string[m_connectingStringLength - 3 + i] = '\0';
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
    if (m_directGameIndex >= 0) {
        WriteToLog("Direct connect game found! Trying to connect...");
        HideSearchDialog();
        JoinRemoteGame(m_directGameIndex);
    } else if (g_currentTick > m_connectingStartTick + m_directConnectTimeout) {
        ShowErrorAndQuit("Game not found.");
    } else {
        UpdateConnectingAnimation();
    }
}
