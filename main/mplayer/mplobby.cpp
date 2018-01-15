#include "mplobby.h"
#include "mplayer.h"
#include "dosipx.h"
#include "qalloc.h"
#include "mppacket.h"
#include "mpcontrol.h"

#pragma GCC diagnostic ignored "-Wparentheses"

static char m_chatLines[MAX_CHAT_LINES][MAX_CHAT_LINE_LENGTH + 1];
static byte m_chatLineColors[MAX_CHAT_LINES];
static int m_currentChatLine;

static WaitingToJoinReport m_joinGamesState;
static LobbyState *m_lobbyState;
static byte m_weAreTheServer;           /* WE ARE THE ROBOTS */
static byte m_connectionError;
static byte m_joinId;
static byte m_gameId;
static byte m_syncDone;
static byte m_inLobby;                  /* will be true while we're in game lobby */

/* GUI callbacks */
static SendWaitingToJoinReport m_waitingToJoinReportFunc;
static bool (*m_joinGameModalFunction)(int status, const char *errorText);
static void (*m_enterGameLobbyFunction)();
static void (*m_updateLobbyFunction)(const LobbyState *state);
static void (*m_disconnectedFunction)();
static void (*m_modalSyncFunction)();
static ShowTeamsMenuFunction m_showTeamsMenuFunction;
static void (*m_onErrorFunction)();     /* called when game exits due to an error */
static void (*m_onGameEndFunction)();   /* called when game ends naturally */
static SetupTeamsOnErrorFunction m_setupTeamsOnErrorFunction;
static SetupTeamsModalFunction m_setupTeamsModalFunction;

/* local info: addresses, names, ids and number of games collected so far */
struct WaitingGame {
    IPX_Address address;
    char name[GAME_NAME_LENGTH + 6 + 1];
    byte id;
};

static int m_numWaitingGames;
static WaitingGame m_waitingGames[MAX_GAMES_WAITING];
static int m_joiningGame;               /* index of a game we're currently trying to join */

#ifdef DEBUG
const char *StateToString(enum MP_State state)
{
    static_assert(ST_MAX == ST_NONE + 10, "States updated, update string conversion also.");
    static char buf[16];
    switch (state) {
    case ST_NONE:
        return "ST_NONE";
    case ST_WAITING_TO_JOIN:
        return "ST_WAITING_TO_JOIN";
    case ST_WAITING_TO_START:
        return "ST_WAITING_TO_START";
    case ST_TRYING_TO_JOIN_GAME:
        return "ST_TRYING_TO_JOIN_GAME";
    case ST_GAME_LOBBY:
        return "ST_GAME_LOBBY";
    case ST_SYNC:
        return "ST_SYNC";
    case ST_PL1_SETUP_TEAM:
        return "ST_PL1_SETUP_TEAM";
    case ST_PL2_SETUP_TEAM:
        return "ST_PL2_SETUP_TEAM";
    case ST_CONFIRMING_KEYS:
        return "ST_CONFIRMING_KEYS";
    case ST_GAME:
        return "ST_GAME";
    default:
        sprintf(buf, "%#x", state);
        return buf;
    }
}
#endif

static const char *joinGameErrors[] = {
    "Unknown error.",
    "Room is full.",
    "You are banned.",
    "Your SWOS++ version is too old.",
    "Room is closed.",
    "Out of resources. Try again.",
    "Game already in progress.",
    "Game recreated. Try again."
};

static enum MP_State m_state = ST_NONE;
static word m_lastRequestTick;
static word m_startSearchTick;
static word m_lastPingTime;
static word m_findGamesTimeout;

#define isPlayer(i)         (m_connectedPlayers[i].isPlayer())
#define isReady(i)          (m_connectedPlayers[i].isReady())
#define isSynced(i)         (m_connectedPlayers[i].isSynced())
#define isControlling(i)    (m_connectedPlayers[i].isControlling())

static NetPlayer m_connectedPlayers[MAX_PLAYERS]; /* keep track of our peers */
static int m_numConnectedPlayers;
static int m_ourPlayerIndex;

static bool LoadTeam(dword teamId, TeamFile *dest);
static LobbyState *InitLobbyState(LobbyState *state);
static LobbyState *PutOurPlayerOnTop(LobbyState *state);
static void CleanUpPlayers();

static void EnterSyncingState(void (*modalSyncFunc)(), ShowTeamsMenuFunction showTeamsMenuFunc);
static void DisconnectClient(int playerIndex, bool sendDisconnect);
static void SendListGamesRequest();
static void SendWaitingGamesReport();

/* === public functions */

/** GetState

    Return current state of our giant state machine.
*/
MP_State GetState()
{
    return m_state;
}

/** SwitchToState

    Switch to a given state.
*/
void SwitchToState(MP_State state)
{
    WriteToLog("Going from %s to %s", StateToString(m_state), StateToString(state));
    m_state = state;
}

/** IsOurPlayerControlling

    Return true if our player is currently in control in setting up lineup menu.
*/
bool IsOurPlayerControlling()
{
    assert(m_ourPlayerIndex >= 0 && m_ourPlayerIndex < (int)sizeofarray(m_connectedPlayers));
    return isControlling(m_ourPlayerIndex);
}

/** GetNumPlayers

    Return number of players that we are currently connected too (includes watchers too).
*/
int GetNumPlayers()
{
    return m_numConnectedPlayers;
}

/** GetPlayer

    Get pointer to player struct at given zero-based index (includes watchers too).
*/
NetPlayer *GetPlayer(int index)
{
    assert(index >= 0 && index < m_numConnectedPlayers);
    return &m_connectedPlayers[index];
}

/** GetOurPlayerIndex

    Return our player index (zero based).
*/
int GetOurPlayerIndex()
{
    return m_ourPlayerIndex;
}

/** GetControllingPlayerAddress

    Return IPX address of a player currently in control. Return null if no one is
    (that should only happen if we are not in play match menu yet).
*/
IPX_Address *GetControllingPlayerAddress()
{
    for (int i = 0; i < m_numConnectedPlayers; i++)
        if (isControlling(i))
            return &m_connectedPlayers[i].address;

    return nullptr;
}

/** GetCurrentPlayerTeamIndex

    Return zero based index of team of current player that's holding the controls in play match menu.
    This is 0 or 1 value, *NOT* player index -- it will mimic index of the team in selectedTeams.
*/
int GetCurrentPlayerTeamIndex()
{
    assert(m_state == ST_PL1_SETUP_TEAM || m_state == ST_PL2_SETUP_TEAM ||
        m_state == ST_CONFIRMING_KEYS || m_state == ST_GAME);

    for (int i = 0, playerIndex = 0; i < m_numConnectedPlayers; i++) {
        if (isControlling(i))
            return playerIndex;

        if (isPlayer(i))
            playerIndex++;

        assert(i <= 1);
    }

    assert(0);      /* something is seriously wrong if we get here */
    return 0;
}

/* Team setting up is canceled, go back to lobby. */
void GoBackToLobby()
{
    WriteToLog("Exit pressed, going back to lobby.");
    SwitchToState(m_weAreTheServer ? ST_WAITING_TO_START : ST_GAME_LOBBY);
}

void InitMultiplayerLobby()
{
    m_lobbyState = (LobbyState *)qAlloc(sizeof(LobbyState));
    assert(m_lobbyState);
    m_connectionError = false;
    m_inLobby = false;
}

void FinishMultiplayerLobby()
{
    qFree(m_lobbyState);
    m_lobbyState = nullptr;
    CleanUpPlayers();
}

void DisbandGame()
{
    if (m_weAreTheServer) {
        WriteToLog("Game disbanded.");
        int length;
        char *packet = CreatePlayerLeftPacket(&length, 0);

        for (int i = 1; i < m_numConnectedPlayers; i++) {
            SetPlayerIndex(packet, i);
            SendSimplePacket(&m_connectedPlayers[i].address, packet, length);
            DisconnectFrom(&m_connectedPlayers[i].address);
        }
    } else {
        DisconnectClient(0, true);
        WriteToLog("Game left.");
    }

    SwitchToState(ST_NONE);
    CancelSendingPackets();
    ReleaseClientMPOptions();
    CleanUpPlayers();
    m_inLobby = false;
}

/* Internal routines */

static void CleanUpPlayers()
{
    for (int i = 0; i < m_numConnectedPlayers; i++) {
        qFree(m_connectedPlayers[i].userTactics);
        m_connectedPlayers[i].userTactics = nullptr;
    }
    m_numConnectedPlayers = 0;
}

/** LoadTeam

    teamId   -  id of team to load
    destTeam -> pointer to buffer that will receive loaded team, if everything OK

    Load specified team into memory, do some basic check if it's ID is valid. Return
    true if everything went well, false for error.
*/
static bool LoadTeam(dword teamId, TeamFile *destTeam)
{
    if (teamId == (dword)-1)
        return false;

    int teamNo = teamId & 0xff, ordinal = (teamId >> 8) & 0xff, handle;

    WriteToLog("Trying to load team %#x, teamNo = %d, ordinal = %d", teamId, teamNo, ordinal);
    if (ordinal > 93) {
        WriteToLog("Too big ordinal. Rejecting team id %#x", teamId);
        return false;
    }

    char *fileName = aDataTeam_nnn;
    if (teamNo == 100)
        fileName = aCustoms_edt;
    else {
        aDataTeam_nnn[10] = teamNo / 100 + '0';
        aDataTeam_nnn[11] = teamNo / 10 % 10 + '0';
        aDataTeam_nnn[12] = teamNo % 10 + '0';
    }

    if ((handle = OpenFile(F_READ_ONLY, fileName)) == -1) {
        WriteToLog("Team id %#x invalid (team file missing).", teamId);
        return false;
    }

    CloseFile(handle);

    D0 = teamNo;
    calla(LoadTeamFile);

    memcpy(destTeam, teamFileBuffer + 2 + ordinal * sizeof(TeamFile), sizeof(TeamFile));
    destTeam->teamStatus = 2;                                                       /* controlled by player */
    *strncpy(destTeam->coachName, GetPlayerNick(), sizeof(destTeam->coachName) - 1) = '\0';  /* set coach name to player nickname */

    ApplySavedTeamData(destTeam);

    WriteToLog("Loaded team '%s' OK.", destTeam->name);
    return true;
}

/* This is an important thing to do, otherwise play match menu will be working with teams in wrong
   locations, and will fail to write changes properly. So make it look like as if team was loaded from
   friendly game team selection. */
static TeamFile *AddTeamToSelectedTeams(const TeamFile *team)
{
    return (TeamFile *)memcpy(&g_selectedTeams[g_numSelectedTeams++], team, sizeof(TeamFile));
}

/* Return player index from IPX address. */
static int FindPlayer(const IPX_Address *node)
{
    int i;

    for (i = 0; i < m_numConnectedPlayers; i++)
        if (AddressMatch(node, &m_connectedPlayers[i].address))
            break;

    return i;
}

static void InitPlayer(NetPlayer *player, const char *name, const char *teamName, const IPX_Address *address, word joinId)
{
    player->name[0] = '\0';
    player->team.teamFileNo = 0;

    if (name)
        *strncpy(player->name, name, NICKNAME_LEN) = '\0';

    if (teamName)
        *strncpy(player->team.name, teamName, sizeof(decltype(player->team)::name) - 1) = '\0';

    player->team.teamFileNo = player->team.teamOrdinal = player->team.globalTeamNumber = -1;
    player->userTactics = nullptr;

    if (address)
        CopyAddress(&player->address, address);

    player->flags = 0;
    player->joinId = joinId;
    player->lastPingTime = 0;
}

/** HandleChatLine

    senderIndex -  index of player that sent the message, or -1 if it's a "system" message
    text        -> text of the message
    color       -  color of the message (from menu palette)

    Add a chat line to our storage and dispatch to clients if we're the server.
    If senderIndex is negative it's a "system" message, usually about some event, such
    as player disconnect.
*/
static void HandleChatLine(int senderIndex, const char *text, byte color)
{
    static int lastSender = -1;
    int startLine = 0, endLine = 0;
    byte colors[2];
    char *linesToSend[2];
    m_currentChatLine = (m_currentChatLine + 1) % MAX_CHAT_LINES;
    colors[0] = colors[1] = color;
    linesToSend[0] = linesToSend[1] = m_chatLines[m_currentChatLine];

    if (m_weAreTheServer && senderIndex >= 0 && senderIndex != lastSender) {
        char *p = strncpy(m_chatLines[m_currentChatLine],
            m_connectedPlayers[senderIndex].getName(), MAX_CHAT_LINE_LENGTH);
        if (p - m_chatLines[m_currentChatLine] < MAX_CHAT_LINE_LENGTH)
            *p++ = ':';
        *p = '\0';
        m_chatLineColors[m_currentChatLine] = colors[0] = 13;
        lastSender = senderIndex;
        m_currentChatLine = (m_currentChatLine + 1) % MAX_CHAT_LINES;
        linesToSend[1] = m_chatLines[m_currentChatLine];
        endLine++;
        color = 2;
    }

    *strncpy(m_chatLines[m_currentChatLine], text, MAX_CHAT_LINE_LENGTH) = '\0';
    m_chatLineColors[m_currentChatLine] = color;

    /* Dispatch chat line(s) to clients. */
    if (m_weAreTheServer) {
        int length;
        for (int i = startLine; i <= endLine; i++) {
            char *packet = CreatePlayerChatPacket(&length, linesToSend[i], colors[i]);
            for (int j = 1; j < m_numConnectedPlayers; j++)
                SendImportantPacket(&m_connectedPlayers[j].address, packet, length);
        }
    }
}

static void DisconnectClient(int playerIndex, bool sendDisconnect)
{
    assert(m_numConnectedPlayers < MAX_PLAYERS);

    if (playerIndex < 0 || playerIndex >= m_numConnectedPlayers) {
        WriteToLog("Invalid player index in disconnect (%d).", playerIndex);
        return;
    }

    int length;
    char *packet = CreatePlayerLeftPacket(&length, playerIndex);

    /* send disconnect packet to offending client, but don't wait for reply
       (connection most likely already lost anyway) */
    if (sendDisconnect)
        SendSimplePacket(&m_connectedPlayers[playerIndex].address, packet, length);

    DisconnectFrom(&m_connectedPlayers[playerIndex].address);
    qFree(m_connectedPlayers[playerIndex].userTactics);
    m_connectedPlayers[playerIndex].userTactics = nullptr;

    if (m_weAreTheServer) {
        for (int i = 1; i < m_numConnectedPlayers; i++)
            if (i != playerIndex)
                SendImportantPacket(&m_connectedPlayers[i].address, packet, length);

        /* shift everything up */
        memmove(m_connectedPlayers + playerIndex, m_connectedPlayers + playerIndex + 1,
            sizeof(m_connectedPlayers[0]) * (m_numConnectedPlayers-- - playerIndex - 1));
    }
}

static void FormatPlayerLeftMessage(char *buf, int playerIndex)
{
    auto end = strcpy(buf, m_connectedPlayers[playerIndex].getName());
    strupr(buf);
    strcpy(end, " LEFT.");
}

static void ServerHandlePlayerLeftPacket(const char *packet, int length, const IPX_Address *node)
{
    assert(m_weAreTheServer);
    int playerIndex;

    if (!UnpackPlayerLeftPacket(packet, length, &playerIndex)) {
        WriteToLog("Rejecting invalid player disconnect packet.");
        return;
    }

    if ((playerIndex = FindPlayer(node)) > m_numConnectedPlayers - 1) {
        WriteToLog("Probably left-over disconnect request received. Ignoring.");
        return;
    }

    WriteToLog("About to disconnect player (by their request), playerIndex = %d, m_numConnectedPlayers = %d",
        playerIndex, m_numConnectedPlayers);
    DisconnectClient(playerIndex, true);
    char buf[32];
    FormatPlayerLeftMessage(buf, playerIndex);
    HandleChatLine(-1, buf, 10);
}

static bool ClientHandlePlayerLeftPacket(const char *packet, int length, const IPX_Address *node)
{
    assert(!m_weAreTheServer);
    int playerIndex;

    if (!UnpackPlayerLeftPacket(packet, length, &playerIndex)) {
        WriteToLog("Malformed player left packet rejected.");
    } else {
        if (!AddressMatch(node, &m_connectedPlayers[0].address)) {
            WriteToLog("Client sent us PT_PLAYER_LEFT [%#.12s]", node);
            return true;
        }

        /* "Don't ask for whom the bell tolls, it tolls for thee..." */
        if (playerIndex == m_ourPlayerIndex) {
            WriteToLog("Server has disconnected us.");
            DisconnectClient(0, true);
            SwitchToState(ST_NONE);
            assert(m_disconnectedFunction);
            m_disconnectedFunction();
            return false;
        } else {
            if (playerIndex > m_numConnectedPlayers) {
                WriteToLog("Invalid player remove request (%d/%d)", playerIndex, m_numConnectedPlayers);
                return true;
            }

            WriteToLog("Removing player %d, num. connected players is %d", playerIndex, m_numConnectedPlayers);
            assert(playerIndex > 0);
            memmove(m_connectedPlayers + playerIndex, m_connectedPlayers + playerIndex + 1,
                sizeof(m_connectedPlayers[0]) * (m_numConnectedPlayers-- - playerIndex - 1));
            if (m_ourPlayerIndex >= m_numConnectedPlayers)
                m_ourPlayerIndex--;
        }
    }

    return true;
}

/***

   Game Lobby menu GUI interfacing.
   =================================

***/

void CreateNewGame(const MP_Options *options, void (*updateLobbyFunc)(const LobbyState *),
    void (*errorFunc)(), void (*onGameEndFunc)(), bool weAreTheServer)
{
    WriteToLog("Creating new IPX game as a %s, state = %#x...", weAreTheServer ? "server" : "client", m_state);
    assert(m_state == ST_NONE || m_state == ST_TRYING_TO_JOIN_GAME);
    SwitchToState(weAreTheServer ? ST_WAITING_TO_START : ST_GAME_LOBBY);

    SetMPOptions(options);

    m_updateLobbyFunction = updateLobbyFunc;
    m_onErrorFunction = errorFunc;
    m_onGameEndFunction = onGameEndFunc;
    m_weAreTheServer = weAreTheServer;

    if (weAreTheServer) {
        m_numConnectedPlayers = 1;    /* self */
        m_ourPlayerIndex = 0;
        m_inLobby = true;

        /* do a full initialization here so as not to forget something */
        InitPlayer(&m_connectedPlayers[0], GetPlayerNick(), nullptr, nullptr, 0);
        GetOurAddress(&m_connectedPlayers[0].address);

        /* if we have the team number, load it */
        auto teamId = GetCurrentTeamId();
        if (teamId != (dword)-1 && !LoadTeam(teamId, &m_connectedPlayers[0].team)) {
            WriteToLog("Failed to load team %#x.", teamId);
            SetCurrentTeamId(-1);
        }

        calla(Rand);
        m_gameId = D0;
        WriteToLog("Game ID is %d", m_gameId);
    }

    for (int i = 0; i < MAX_CHAT_LINES; i++) {
        m_chatLines[i][0] = '\0';
        m_chatLineColors[i] = 2;
    }

    m_currentChatLine = -1;
}

void UpdateMPOptions(const MP_Options *newOptions)
{
    assert(m_weAreTheServer);

    if (m_weAreTheServer && !CompareMPOptions(newOptions)) {
        int length;
        char *packet = CreateOptionsPacket(&length, newOptions);

        /* signal a change in options (server-only) */
        for (int i = 1; i < m_numConnectedPlayers; i++)
            SendImportantPacket(&m_connectedPlayers[i].address, packet, length);
    }

    SetMPOptions(newOptions);
}

static void SetFlags(int bit, bool set)
{
    char *packet;
    int length;
    byte oldFlags = m_connectedPlayers[m_ourPlayerIndex].flags;
    m_connectedPlayers[m_ourPlayerIndex].flags = ((oldFlags & ~(1 << bit))) | (((set != 0) << bit));

    if (oldFlags != m_connectedPlayers[m_ourPlayerIndex].flags) {
        packet = CreatePlayerFlagsPacket(&length, m_connectedPlayers[m_ourPlayerIndex].flags, m_ourPlayerIndex);
        if (m_weAreTheServer)
            for (int i = 1; i < m_numConnectedPlayers; i++)
                SendImportantPacket(&m_connectedPlayers[i].address, packet, length);
        else
            SendImportantPacket(&m_connectedPlayers[0].address, packet, length);
    }
}

void SetPlayerOrWatcher(bool isWatcher)
{
    SetFlags(0, isWatcher);
}

void SetPlayerReadyState(bool isReady)
{
    SetFlags(1, isReady);
}

/** SetTeam

    newTeam -> new selected team
    teamId  -  id of new team

    Player has selected (potentially) new team. Set it all up, notify the server or
    all the clients if necessary.
*/
char *SetTeam(TeamFile *newTeam, dword teamId)
{
    /* must copy it to selected teams too */
    memcpy(&m_connectedPlayers[m_ourPlayerIndex].team, newTeam, sizeof(TeamFile));
    SetCurrentTeamId(teamId);
    ApplySavedTeamData(newTeam);

    if (m_inLobby) {
        int length;
        char *packet = CreatePlayerTeamChangePacket(&length, 0, newTeam->name);

        if (m_weAreTheServer)
            for (int i = 1; i < m_numConnectedPlayers; i++)
                SendImportantPacket(&m_connectedPlayers[i].address, packet, length);
        else
            SendImportantPacket(&m_connectedPlayers[0].address, packet, length);
    }

    return newTeam->name;
}

/* User has typed a chat line. */
void AddChatLine(const char *line)
{
    if (!line || !*line)
        return;

    if (m_weAreTheServer) {
        HandleChatLine(0, line, 2);
    } else {
        int length;
        char *packet = CreatePlayerChatPacket(&length, line, 0);
        SendImportantPacket(&m_connectedPlayers[0].address, packet, length);
    }
}

/** CanGameStart

    Test if conditions are fulfilled for starting a game. Those can be summarized as:
    - having exactly 2 players
    - both players must be ready
    - both players must have their teams selected
*/
bool32 CanGameStart()
{
    int numReadyPlayers = 0, numNonReadyPlayers = 0;
    if (!m_weAreTheServer)
        return false;

    for (int i = 0; i < m_numConnectedPlayers; i++) {
        if (isPlayer(i)) {
            if (isReady(i) && m_connectedPlayers[i].team.name[0] != '\0')
                numReadyPlayers++;  /* must be player, ready and have a team */
            else
                numNonReadyPlayers++;
        }
    }

    return numReadyPlayers == 2 && numNonReadyPlayers == 0;
}

/** SetupTeams

    Start game was pressed in game lobby. Commence synchronization process, where players exchange
    their teams and custom tactics. Prerequisite of setup teams menu.
*/
void SetupTeams(void (*modalSyncFunc)(), ShowTeamsMenuFunction showTeamsMenuFunc,
    SetupTeamsModalFunction setupTeamsModalFunc, SetupTeamsOnErrorFunction setupTeamsOnErrorFunc)
{
    WriteToLog("Synchronizing... modalSyncFunc = %#x, showTeamsMenuFunc = %#x", modalSyncFunc, showTeamsMenuFunc);
    WriteToLog("setupTeamsModalFunc = %#x, setupTeamsOnErrorFunc = %#x", setupTeamsModalFunc, setupTeamsOnErrorFunc);

    m_setupTeamsModalFunction = setupTeamsModalFunc;
    m_setupTeamsOnErrorFunction = setupTeamsOnErrorFunc;

    const IPX_Address *addresses[MAX_PLAYERS - 1];
    for (int i = 1; i < m_numConnectedPlayers; i++)
        addresses[i - 1] = &m_connectedPlayers[i].address;

    byte randomVariables[32];
    int randVarsSize = sizeof(randomVariables);
    GetRandomVariables(randomVariables, &randVarsSize);
    assert(randVarsSize <= (int)sizeof(randomVariables));
    WriteToLog("Sending random variables: [%#.*s]", randVarsSize, randomVariables);

    int length;
    char *packet = CreateSyncPacket(&length, addresses, m_numConnectedPlayers - 1, randomVariables, randVarsSize);
    for (int i = 1; i < m_numConnectedPlayers; i++)
        SendImportantPacket(addresses[i - 1], packet, length);

    EnterSyncingState(modalSyncFunc, showTeamsMenuFunc);
}

/***

   Game lobby menu internal logic.
   ================================

***/

/* Called from main network loop to dispatch server traffic in the game lobby. */
static void CheckIncomingServerLobbyTraffic()
{
    char *packet, *response, playerName[NICKNAME_LEN + 1], teamName[MAX_TEAM_NAME_LEN + 1];
    char chatLine[MAX_CHAT_LINE_LENGTH + 1], buf[32];
    int i, length, respLength, playerIndex, flags;
    word networkVersion, networkSubversion;
    byte joinId, receivedGameId, color;
    IPX_Address node;

    /* ignore lost packets due to low memory in hope they'll send again when we have more memory */
    if (!(packet = ReceivePacket(&length, &node)))
        return;

    switch (GetRequestType(packet, length)) {
    case PT_GET_GAME_INFO:  /* someone's querying about open games */
        WriteToLog("Received game info request");
        response = CreateGameInfoPacket(&respLength, m_numConnectedPlayers, GetGameName(), m_gameId);
        WriteToLog("Responding to enumerate games request.");
        SendSimplePacket(&node, response, respLength);
        break;

    case PT_JOIN_GAME:      /* someone's trying to join */
        WriteToLog("Received join game request");
        if (UnpackJoinGamePacket(packet, length, &networkVersion, &networkSubversion,
            &joinId, &receivedGameId, playerName, teamName)) {
            WriteToLog("Join ID = %d, game ID = %d, our game ID = %d, player name = %s, team name = %s",
                joinId, receivedGameId, m_gameId, playerName, teamName);
            if (receivedGameId != m_gameId) {
                WriteToLog("Invalid game ID, refusing connection.");
                response = CreateRefuseJoinGamePacket(&respLength, GR_INVALID_GAME);
                SendSimplePacket(&node, response, respLength);
                break;
            }

            if (m_numConnectedPlayers < MAX_PLAYERS) {
                if (networkVersion >= NETWORK_VERSION) {
                    /* we got a possible new player */
                    NetPlayer *n = m_connectedPlayers + m_numConnectedPlayers;
                    const IPX_Address *playerAddresses[MAX_PLAYERS];
                    /* see if we already have them */
                    if ((playerIndex = FindPlayer(&node)) < m_numConnectedPlayers) {
                        /* there's a possibility that the client crashed but we haven't noticed it,
                           in that case we might wrongfully disallow them to join; so we introduce join
                           id - client will generate one randomly and send it when trying to join, that's
                           how we will be able to differentiate if it's a different join attempt */
                        if (joinId == m_connectedPlayers[playerIndex].joinId) {
                            WriteToLog("Ignoring superfluous join request (%s), id = %d",
                                m_connectedPlayers[playerIndex].getName(), joinId);
                            qFree(packet);
                            return;
                        } else {
                            /* inform other clients about disconnect, but don't send disconnect to the client */
                            WriteToLog("Re-join detected, old id = %d, new id = %d",
                                m_connectedPlayers[playerIndex].joinId, joinId);
                            DisconnectClient(playerIndex, false);
                            n = m_connectedPlayers + m_numConnectedPlayers;
                        }
                    }

                    InitPlayer(n, playerName, teamName, &node, joinId);
                    WriteToLog("Joining new player %s to the game, with join id %d.", n->name, n->joinId);
                    ConnectTo(&node);

                    for (i = 0; i < m_numConnectedPlayers + 1; i++)
                        playerAddresses[i] = &m_connectedPlayers[i].address;

                    InitLobbyState(m_lobbyState);
                    response = CreateJoinedGameOkPacket(&respLength, m_lobbyState, playerAddresses);
                    WriteToLog("Responding positively to join game request.");
                    SendImportantPacket(&node, response, respLength);

                    /* inform other players that the new one has joined */
                    response = CreatePlayerJoinedPacket(&respLength, playerName, teamName);

                    for (i = 1; i < m_numConnectedPlayers; i++)
                        SendImportantPacket(&m_connectedPlayers[i].address, response, respLength);

                    auto end = strcpy(buf, playerName);
                    strupr(buf);
                    strcpy(end, " JOINED.");
                    HandleChatLine(-1, buf, 14);
                    m_numConnectedPlayers++;
                    qFree(packet);
                    return;
                } else {
                    response = CreateRefuseJoinGamePacket(&respLength, GR_WRONG_VERSION);
                }
            } else {
                response = CreateRefuseJoinGamePacket(&respLength, GR_ROOM_FULL);
            }
        } else {
            WriteToLog("Malformed join game packet rejected.");
            break;
        }
        SendSimplePacket(&node, response, respLength);
        break;

    case PT_PLAYER_TEAM:
        if (!UnpackPlayerTeamChangePacket(packet, length, &playerIndex, teamName)) {
            WriteToLog("Invalid team change packet rejected.");
            break;
        }

        if ((playerIndex = FindPlayer(&node)) > m_numConnectedPlayers) {
            WriteToLog("Ignoring left-over player team packet.");
            break;
        }

        /* set our internal state */
        *strncpy(m_connectedPlayers[playerIndex].team.name, teamName, MAX_TEAM_NAME_LEN) = '\0';

        /* reuse that packet to notify other clients */
        SetPlayerIndex(packet, playerIndex);
        for (i = 1; i < m_numConnectedPlayers; i++)
            if (i != playerIndex)
                SendImportantPacket(&m_connectedPlayers[i].address, packet, length);
        break;

    case PT_PLAYER_FLAGS:
        if (!UnpackPlayerFlagsPacket(packet, length, &playerIndex, &flags)) {
            WriteToLog("Invalid flags packet rejected.");
            break;
        }

        if ((playerIndex = FindPlayer(&node)) > m_numConnectedPlayers) {
            WriteToLog("Ignoring left-over flags packet.");
            break;
        }

        m_connectedPlayers[playerIndex].flags = flags;
        SetPlayerIndex(packet, playerIndex);

        for (i = 1; i < m_numConnectedPlayers; i++)
            if (i != playerIndex)
                SendImportantPacket(&m_connectedPlayers[i].address, packet, length);

        break;

    case PT_OPTIONS:
        /* this shouldn't happen (unless rogue client :P) */
        WriteToLog("Weird, client is sending us options, from address [%#.12s]", &node);
        break;

    case PT_PLAYER_LEFT:
        ServerHandlePlayerLeftPacket(packet, length, &node);
        break;

    case PT_CHAT_LINE:
        //TODO:flood protection
        if (!UnpackPlayerChatPacket(packet, length, chatLine, &color)) {
            WriteToLog("Ignoring invalid chat line packet.");
            break;
        }

        playerIndex = FindPlayer(&node);

        if (playerIndex > 0 && playerIndex < m_numConnectedPlayers)
            HandleChatLine(playerIndex, chatLine, 2);
        else
            WriteToLog("Weird, received chat line from a disconnected player.");

        break;

    case PT_PING:
        /* ignore */
        break;

    case PT_GAME_CONTROLS:
        /* possible last packet needing ack */
        SendSimplePacket(&node, packet, length);
        break;

    default:
        WriteToLog("Unknown packet type received, %#x, length = %d", GetRequestType(packet, length), length);
        HexDumpToLog(packet, length, "unknown packet");
    }

    qFree(packet);
}

/* Send ping packet to clients periodically, so we can detect client disconnects even if they're idle. */
static void SendPingPackets()
{
    word timeout = GetNetworkTimeout(), currentTime = g_currentTick, pingPacket = PT_PING;
    for (int i = 1; i < m_numConnectedPlayers; i++) {
        if (currentTime - timeout > m_connectedPlayers[i].lastPingTime) {
            SendImportantPacket(&m_connectedPlayers[i].address, (char *)&pingPacket, sizeof(word));
            m_connectedPlayers[i].lastPingTime = currentTime;
        }
    }
}

static void PingServer()
{
    word timeout = GetNetworkTimeout(), currentTime = g_currentTick, pingPacket = PT_PING;
    if (currentTime - timeout > m_lastPingTime) {
        SendImportantPacket(&m_connectedPlayers[0].address, (char *)&pingPacket, sizeof(pingPacket));
        m_lastPingTime = currentTime;
    }
}

/* Network loop in game lobby for clients. */
static bool CheckIncomingClientLobbyTraffic()
{
    char *packet, playerName[NICKNAME_LEN + 1], playerTeamName[MAX_TEAM_NAME_LEN + 1];
    char chatLine[MAX_CHAT_LINE_LENGTH + 1];
    int i, length, playerIndex, flags, newPlayerIndex;
    IPX_Address node, *addresses[MAX_PLAYERS - 1];
    byte color;

    if (!(packet = ReceivePacket(&length, &node)))
        return true;

    /* only listen to the server */
    //TODO:generate unique random id on log-in
    if (!AddressMatch(&node, &m_connectedPlayers[0].address)) {
        WriteToLog("Rejecting packet probably from other client (non-server): [%#.12s].", &node);
        qFree(packet);
        return true;
    }

    MP_Options tmpOptions;
    switch (GetRequestType(packet, length)) {
    case PT_OPTIONS:
        if (!UnpackOptionsPacket(packet, length, &tmpOptions))
            WriteToLog("Malformed options packet rejected.");
        else
            SetMPOptions(&tmpOptions);
        break;

    case PT_PLAYER_FLAGS:
        if (!UnpackPlayerFlagsPacket(packet, length, &playerIndex, &flags) || playerIndex >= MAX_PLAYERS)
            WriteToLog("Malformed flags packet rejected.");
        else
            m_connectedPlayers[playerIndex].flags = flags;

        assert(playerIndex >= 0);
        break;

    case PT_PLAYER_JOINED:
        if (!UnpackPlayerJoinedPacket(packet, length, playerName, playerTeamName)) {
            WriteToLog("Malformed player joined packet rejected.");
        } else {
            newPlayerIndex = m_numConnectedPlayers++;
            InitPlayer(m_connectedPlayers + newPlayerIndex, playerName, playerTeamName, &node, 0x1ff);
            WriteToLog("New player joined, name: '%s', team name: '%s'", playerName, playerTeamName);
        }
        break;

    case PT_PLAYER_LEFT:
        if (!ClientHandlePlayerLeftPacket(packet, length, &node)) {
            qFree(packet);
            return false;
        }
        break;

    case PT_PLAYER_TEAM:
        if (!UnpackPlayerTeamChangePacket(packet, length, &playerIndex, playerTeamName)) {
            WriteToLog("Malformed player team change packet rejected.");
        } else {
            assert(playerIndex >= 0 && playerIndex < m_numConnectedPlayers);
            *strncpy(m_connectedPlayers[playerIndex].team.name, playerTeamName, MAX_TEAM_NAME_LEN) = '\0';
        }
        break;

    case PT_PING:
        /* ignore */
        //WriteToLog("Responded to ping from the server... time is %d", currentTick);
        break;

    case PT_CHAT_LINE:
        if (UnpackPlayerChatPacket(packet, length, chatLine, &color)) {
            HandleChatLine(-1, chatLine, color);
        } else
            WriteToLog("Invalid chat line packet ignored.");
        break;

    case PT_SYNC:
        for (i = 1; i < m_numConnectedPlayers; i++)
            addresses[i - 1] = &m_connectedPlayers[i].address;

        /* re-use chat line buffer to hold random variables content */
        i = sizeof(chatLine);

        if (!UnpackSyncPacket(packet, length, addresses, m_numConnectedPlayers - 1, (byte *)chatLine, &i)) {
            WriteToLog("Invalid sync packet rejected.");
        } else {
            WriteToLog("Received random variables: [%#.*s]", i, chatLine);
            /* apply random variables to keep the both clients in perfect sync */
            SetRandomVariables(chatLine, i);
            /* connect to other player(s) to accept their team data */
            for (i = 1; i < m_numConnectedPlayers; i++)
                if (i != m_ourPlayerIndex && (isPlayer(i) || isPlayer(m_ourPlayerIndex)))
                    ConnectTo(&m_connectedPlayers[i].address);
            /* players send their teams and tactics */
            if (isPlayer(m_ourPlayerIndex)) {
                char *packetToSend = CreateTeamAndTacticsPacket(&length,
                    &m_connectedPlayers[m_ourPlayerIndex].team, GetUserMpTactics());
                for (i = 0; i < m_numConnectedPlayers; i++)
                    if (i != m_ourPlayerIndex) {
                        //maybe some resend flag if it fails?
                        SendImportantPacket(&m_connectedPlayers[i].address, packetToSend, length);
                    }
            }
            /* note that we might reject some packets if they arrive before we connect
               to those clients, but they will resend so everything should be fine */
        }
        EnterSyncingState(m_modalSyncFunction, m_showTeamsMenuFunction);
        break;

    case PT_GAME_CONTROLS:
        /* this could be left over last packet needing ack, simply return it to sender */
        SendSimplePacket(&node, packet, length);
        break;

    default:
        WriteToLog("Unexpected packet %#hx ignored", *(word *)packet);
    }

    qFree(packet);
    return true;
}

static bool GotAllTeamsAndTactics()
{
    int numPlayers = 0;

    for (int i = 0; i < m_numConnectedPlayers; i++) {
        if (isPlayer(i)) {
            if (!m_connectedPlayers[i].userTactics)
                return false;
            numPlayers++;
        }
    }

    return !m_weAreTheServer || numPlayers > 1;
}

/* Multiplayer game just ended. Get the results and go back to corresponding state. */
void GameFinished()
{
    assert(m_onGameEndFunction && m_onErrorFunction);

    byte gameStatus = GetGameStatus();
    if (gameStatus != GS_GAME_DISCONNECTED && gameStatus != GS_WATCHER_ABORTED) {
        GoBackToLobby();
        assert(m_onGameEndFunction);
        m_onGameEndFunction();
    } else {
        DisbandGame();
        assert(m_onErrorFunction);
        m_onErrorFunction();
    }
}

static void MenuTransitionAfterTheGame()
{
    /* must refresh menu before the fade in, and after menu conversion */
    if (m_state == ST_GAME_LOBBY || m_state == ST_WAITING_TO_START)
        m_updateLobbyFunction(PutOurPlayerOnTop(InitLobbyState(m_lobbyState)));
}

/** HandleSyncTimeouts

    Handle resending of unacknowledged packets and network failures while syncing.
*/
static bool HandleSyncTimeouts()
{
    if (UnAckPacket *timedOutPacket = ResendUnacknowledgedPackets()) {
        int i = FindPlayer(&timedOutPacket->address);
        if (i >= m_numConnectedPlayers)
            DisconnectFrom(&timedOutPacket->address);   /* just in case */

        if (m_weAreTheServer) {
            WriteToLog("Client [%#.12s] disconnected during the sync.", &timedOutPacket->address);
            DisconnectClient(i, false);
            char buf[32];
            FormatPlayerLeftMessage(buf, i);
            HandleChatLine(-1, buf, 10);
        } else {
            WriteToLog("Timed out packet from [%#.12s]", &timedOutPacket->address);
            word syncPacket = PT_SYNC_FAIL;

            if (i == 0) {
                SendSimplePacket(&m_connectedPlayers[0].address, (char *)&syncPacket, sizeof(word));
                DisconnectClient(0, true);  /* we are disconnected from the server */
                CancelSendingPackets();
                SwitchToState(ST_NONE);
                assert(m_disconnectedFunction);
                m_disconnectedFunction();
                IPX_OnIdle();
                return false;
            } else {
                /* return sync fail to server */
                m_syncDone = true;
                SendImportantPacket(&m_connectedPlayers[0].address, (char *)&syncPacket, sizeof(word));
                DisconnectClient(i, true);
            }
        }
    }

    return true;
}

/** SyncFailed

    Timeout has expired and we haven't managed to sync everybody, so return to the lobby.
*/
static void SyncFailed()
{
    word syncPacket = PT_SYNC_FAIL;

    if (m_weAreTheServer) {
        for (int i = 1; i < m_numConnectedPlayers; i++)
            SendImportantPacket(&m_connectedPlayers[i].address, (char *)&syncPacket, sizeof(word));
        HandleChatLine(-1, "SYNC FAILED: TIMEOUT", 10);
        SwitchToState(ST_WAITING_TO_START);
    } else {
        SendImportantPacket(&m_connectedPlayers[0].address, (char *)&syncPacket, sizeof(word));
        SwitchToState(ST_GAME_LOBBY);
    }

    CancelSendingPackets();
}

static void ShowTeamsMenu(TeamFile *team1, TeamFile *team2)
{
    assert(m_showTeamsMenuFunction && team1 && team2);
    g_numSelectedTeams = 0;     /* reset this or we will be adding duplicate teams after 1st game */
    team1 = AddTeamToSelectedTeams(team1);
    team2 = AddTeamToSelectedTeams(team2);
    InitializeNetworkKeys();
    m_showTeamsMenuFunction(team1, team2, GetCurrentPlayerTeamIndex);
    MenuTransitionAfterTheGame();
    IPX_OnIdle();
}

/** ServerSyncSuccess

    If we are server check if everybody is synced, and if so switch into next phase - team setup.
*/
static bool32 ServerSyncSuccess()
{
    if (m_weAreTheServer) {
        int i;

        for (i = 0; i < m_numConnectedPlayers; i++)
            if (!isSynced(i))
                break;

        /* Number of connected players might go down if some disconnect during the sync. */
        if (m_numConnectedPlayers > 1 && i >= m_numConnectedPlayers) {
            /* notify clients of successful end of operation */
            WriteToLog("Sync completed successfully. Notifying clients.");

            TeamFile *team1 = nullptr, *team2 = nullptr;
            if (isPlayer(0))
                team1 = &m_connectedPlayers[0].team;

            word syncPacket = PT_SYNC_OK;
            for (i = 1; i < m_numConnectedPlayers; i++) {
                SendImportantPacket(&m_connectedPlayers[i].address, (char *)&syncPacket, sizeof(word));
                if (isPlayer(i)) {
                    if (!team1)
                        team1 = &m_connectedPlayers[i].team;
                    else if (!team2)
                        team2 = &m_connectedPlayers[i].team;
                }
            }

            SwitchToState(ST_PL1_SETUP_TEAM);
            ShowTeamsMenu(team1, team2);

            return true;
        }
    }

    return false;
}

static bool32 HandleSyncOkPacket(char *packet, const IPX_Address& node)
{
    /* check source */
    int sourcePlayer = FindPlayer(&node);

    if (m_weAreTheServer) {
        if (sourcePlayer >= m_numConnectedPlayers) {
            WriteToLog("Received PT_SYNC_OK from non-connected player.");
            return true;
        }
    } else {
        if (sourcePlayer != 0) {
            WriteToLog("Someone other than server sending us sync OK packet. Ignoring.");
            return true;
        }
    }

    int controllingPlayer = -1;
    /* give control to first player, and clear all others */
    for (int i = 0; i < m_numConnectedPlayers; i++) {
        if (controllingPlayer < 0 && isPlayer(i)) {
            m_connectedPlayers[i].setControlling();
            controllingPlayer = i;
        } else
            m_connectedPlayers[i].resetControlling();
    }

    assert(controllingPlayer >= 0);

    /* in case someone dropped */
    if (controllingPlayer < 0)
        return true;

    /* make controlling player tactics current */
    assert(m_connectedPlayers[controllingPlayer].userTactics);
    memcpy(USER_A, m_connectedPlayers[controllingPlayer].userTactics, TACTIC_SIZE * 6);

    if (m_weAreTheServer) {
        m_connectedPlayers[sourcePlayer].setSynced();
    } else {
        WriteToLog("Sync completed successfully.");
        SwitchToState(ST_PL1_SETUP_TEAM);
        qFree(packet);

        TeamFile *team1 = nullptr, *team2 = nullptr;
        for (int i = 0; i < m_numConnectedPlayers; i++) {
            if (isPlayer(i)) {
                if (!team1)
                    team1 = &m_connectedPlayers[i].team;
                else if (!team2)
                    team2 = &m_connectedPlayers[i].team;
            }
        }

        ShowTeamsMenu(team1, team2);
        return false;
    }

    return true;
}

/** SyncOnIdle

    Handle state transition during synchronization - when teams and tactics are
    exchanged among all clients. Server supervises the process and issues final
    result when everything is finished.
    Return true if we are moving out of sync state, be it success or not.
*/
static bool32 SyncOnIdle()
{
    char *packet;
    int i, packetLen;
    IPX_Address node;
    word timeout = GetNetworkTimeout();
    word syncPacket = PT_SYNC_FAIL;
    word packetType;

    if (packet = ReceivePacket(&packetLen, &node)) {
        switch (packetType = GetRequestType(packet, packetLen)) {
        case PT_TEAM_AND_TACTICS:
            if ((i = FindPlayer(&node)) >= m_numConnectedPlayers)
                WriteToLog("Received PT_TEAM_AND_TACTICS from a non-connected player [%#.12s].", &node);
            else if (!isPlayer(i))
                WriteToLog("Received PT_TEAM_AND_TACTICS from a watcher.");
            else {
                qFree(m_connectedPlayers[i].userTactics);
                if (m_connectedPlayers[i].userTactics = (Tactics *)qAlloc(sizeof(Tactics) * 6)) {
                    if (!UnpackTeamAndTacticsPacket(packet, packetLen,  &m_connectedPlayers[i].team, m_connectedPlayers[i].userTactics))
                        WriteToLog("Malformed PT_TEAM_AND_TACTICS packet received.");
                } else {
                    //TODO:fail the sync
                    return false;
                }
            }
            break;

        case PT_SYNC_OK:
            if (!HandleSyncOkPacket(packet, node))
                return false;
            break;

        case PT_PLAYER_LEFT:
            // are we server? source?
            if (m_weAreTheServer)
                ServerHandlePlayerLeftPacket(packet, packetLen, &node);
            else {
                if (!ClientHandlePlayerLeftPacket(packet, packetLen, &node)) {
                    qFree(packet);
                    return false;
                }
            }
            break;

        case PT_SYNC_FAIL:
            if (m_weAreTheServer) {
                /* someone failed */
                if ((i = FindPlayer(&node)) >= m_numConnectedPlayers)
                    WriteToLog("Received PT_SYNC_FAIL from non-connected player.");
                else {
                    /* send fail to everyone and go back to lobby */
                    for (i = 1; i < m_numConnectedPlayers; i++)
                        SendImportantPacket(&m_connectedPlayers[i].address, (char *)&syncPacket, sizeof(word));
                    SwitchToState(ST_WAITING_TO_START);
                    qFree(packet);
                    return true;
                }
            } else if (packetType == PT_SYNC_FAIL) {
                assert(m_ourPlayerIndex != 0);
                if (AddressMatch(&m_connectedPlayers[0].address, &node)) {
                    /* go back to lobby */
                    SwitchToState(ST_GAME_LOBBY);
                    qFree(packet);
                    return true;
                } else
                    WriteToLog("Weird, received PT_SYNC_FAIL from another client.");
            }
            break;
        case PT_PING:
            /* ignore */
            break;
        default:
            WriteToLog("Ignoring packet %#02x while syncing.", GetRequestType(packet, packetLen));
            break;
        }
    }
    qFree(packet);

    if (!m_syncDone && GotAllTeamsAndTactics()) {
        if (!m_weAreTheServer) {
            syncPacket = PT_SYNC_OK;
            SendImportantPacket(&m_connectedPlayers[0].address, (char *)&syncPacket, sizeof(word));
        } else
            m_connectedPlayers[0].setSynced();
        m_syncDone = true;
    }

    if (ServerSyncSuccess())
        return false;

    if (g_currentTick > m_lastRequestTick + 3 * timeout / 2) {
        /* time's up */
        SyncFailed();
        return true;
    }

    if (HandleSyncTimeouts()) {
        m_modalSyncFunction();
        IPX_OnIdle();
    }

    return false;
}

/* Create lobby state to send to a connecting player or GUI. */
static LobbyState *InitLobbyState(LobbyState *state)
{
    assert(state);
    state->numPlayers = m_numConnectedPlayers;
    MP_Options mpOptions;
    state->options = *GetMPOptions(&mpOptions);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        state->playerNames[i] = m_connectedPlayers[i].getName();
        state->playerFlags[i] = m_connectedPlayers[i].flags;
        state->playerTeamsNames[i] = m_connectedPlayers[i].team.name;
    }

    int currentOutputLine = (m_currentChatLine + 1) % MAX_CHAT_LINES;
    for (int i = 0; i < MAX_CHAT_LINES; i++) {
        state->chatLines[i] = m_chatLines[currentOutputLine];
        state->chatLineColors[i] = m_chatLineColors[currentOutputLine];
        currentOutputLine = (currentOutputLine + 1) % MAX_CHAT_LINES;
    }

    return state;
}

static void SwapLobbyStatePlayerSpot(LobbyState *state, int i1, int i2)
{
    assert(state);
    int iTmp;
    char *pTmp;
    pTmp = state->playerNames[i1];
    state->playerNames[i1] = state->playerNames[i2];
    state->playerNames[i2] = pTmp;
    pTmp = state->playerTeamsNames[i1];
    state->playerTeamsNames[i1] = state->playerTeamsNames[i2];
    state->playerTeamsNames[i2] = pTmp;
    iTmp = state->playerFlags[i1];
    state->playerFlags[i1] = state->playerFlags[i2];
    state->playerFlags[i2] = iTmp;
}

/** PutOurPlayerOnTop

    state -> current lobby state

    Always make sure our player appears to be first to GUI.
    Also make server appear to be second. Rest of the player are sorted by
    their normal positions. Doesn't apply if we are the server (server should
    always be at 0-th spot anyway).
*/
static LobbyState *PutOurPlayerOnTop(LobbyState *state)
{
    assert(state);

    if (!m_weAreTheServer) {
        SwapLobbyStatePlayerSpot(state, 0, m_ourPlayerIndex);
        for (int i = m_ourPlayerIndex - 1; i >= 1; i--)
            SwapLobbyStatePlayerSpot(state, i, i + 1);
    }

    return state;
}

/***

   Join game menu GUI interfacing.
   ===============================

***/

/** Called by GUI upon entering join game menu. We are given a function which
    will be used to update GUI after any changes. Start off with refreshing
    game list automatically, as if user pressed refresh button.
*/
void EnterWaitingToJoinState(SendWaitingToJoinReport updateFunc, int findGamesTimeout)
{
    assert(m_state == ST_NONE || m_state == ST_WAITING_TO_JOIN || m_state == ST_TRYING_TO_JOIN_GAME);

    SwitchToState(ST_WAITING_TO_JOIN);
    m_joiningGame = -1;

    if (updateFunc)
        m_waitingToJoinReportFunc = updateFunc;

    /* initialize state */
    m_joinGamesState.refreshing = true;
    m_findGamesTimeout = findGamesTimeout ? findGamesTimeout : GetNetworkTimeout();
    m_startSearchTick = m_lastRequestTick = g_currentTick;
    m_numWaitingGames = 0;  /* clear it in case some games don't exist anymore */

    /* announce us to the world */
    SendListGamesRequest();
    SendWaitingGamesReport();
}

/* Called by GUI when the user is requesting games list refresh. */
void RefreshList()
{
    if (!m_joinGamesState.refreshing)
        EnterWaitingToJoinState();
}

/* User is leaving join games menu, cease all network activity. */
void LeaveWaitingToJoinState()
{
    SwitchToState(ST_NONE);
    m_joinGamesState.refreshing = false;
    m_numWaitingGames = 0;
    CancelSendingPackets();
}

/** JoinGame

    in:
        index                 -  index into waiting games, of the game we're trying to connect to
        modalUpdateFunc       -> callback that controls GUI behaviour during connecting to the lobby, repeatedly called
        enterGameLobbyFunc    -> callback to notify when the lobby has been successfully joined
        disconnectedFunc      -> callback to notify if we're disconnected from the lobby
        modalSyncFunc         -> callback that gets called periodically during the sync phase, when idle
        showTeamsMenuFunc     -> callback to notify when sync phase is over, GUI should take user to team setup
        onErrorFunc           -> callback to notify when game ends with error (will not be notified if there is no error)
        onGameEndFunc         -> callback to notify when game ends properly (will not be notified if error)
        setupTeamsModalFunc   -> callback to notify while we are waiting for other player to confirm the keys we sent
        setupTeamsOnErrorFunc -> callback to notify when timeout occurs while setting up teams

    Called from GUI to notify us that the player wants to join the game with the given index.
    Various callbacks are provided for GUI to be notified in various stages as the process proceeds.
*/
void JoinGame(int index, bool (*modalUpdateFunc)(int, const char *),
    void (*enterGameLobbyFunc)(), void (*disconnectedFunc)(), void (*modalSyncFunc)(),
    ShowTeamsMenuFunction showTeamsMenuFunc, void (*onErrorFunc)(), void (*onGameEndFunc)(),
    SetupTeamsModalFunction setupTeamsModalFunc, SetupTeamsOnErrorFunction setupTeamsOnErrorFunc)
{
    assert(index >= 0 && index < MAX_GAMES_WAITING);
    m_joiningGame = index;

    m_joinGameModalFunction = modalUpdateFunc;
    m_enterGameLobbyFunction = enterGameLobbyFunc;
    m_disconnectedFunction = disconnectedFunc;
    m_modalSyncFunction = modalSyncFunc;
    m_showTeamsMenuFunction = showTeamsMenuFunc;
    m_onErrorFunction = onErrorFunc;
    m_onGameEndFunction = onGameEndFunc;
    m_setupTeamsModalFunction = setupTeamsModalFunc;
    m_setupTeamsOnErrorFunction = setupTeamsOnErrorFunc;

    ConnectTo(&m_waitingGames[index].address);      /* assume connected until proven otherwise */
    CopyAddress(&m_connectedPlayers[0].address, &m_waitingGames[index].address);

    if (!LoadTeam(GetCurrentTeamId(), &m_connectedPlayers[0].team)) {  /* try to load the player's team */
        m_connectedPlayers[0].team.name[0] = '\0';
        SetCurrentTeamId(-1);
    }

    int length;
    char *packet = CreateJoinGamePacket(&length, m_joinId = GetTickCount(), m_waitingGames[index].id,
        GetPlayerNick(), m_connectedPlayers[0].team.name);

    m_lastRequestTick = g_currentTick;      /* record to track timeout in connect */
    m_lastPingTime = 0;
    m_connectionError = false;
    SendSimplePacket(&m_waitingGames[index].address, packet, length);

    SwitchToState(ST_TRYING_TO_JOIN_GAME);
}

/***

   Join game menu internal logic.
   ==============================

***/

/** SendListGamesRequest

    Send broadcast request to which all open games on the network should respond.
*/
static void SendListGamesRequest()
{
    int length;
    char *request = CreateGetGameInfoPacket(&length);
    WriteToLog("Sending broadcast request to list games, last tick is %d.", m_lastRequestTick);
    SendBroadcastPacket(request, length);
}

/* Send state report for GUI to update. */
static void SendWaitingGamesReport()
{
    int i;
    for (i = 0; i < m_numWaitingGames; i++)
        m_joinGamesState.waitingGames[i] = m_waitingGames[i].name;

    m_joinGamesState.waitingGames[i] = (char *)-1;      /* terminate with -1    */
    m_waitingToJoinReportFunc(&m_joinGamesState);       /* dispatch to the GUI  */
}

/** UpdateGameList

    Send a broadcast request to find open games while we're in refresh mode, and
    at most once per defined interval (should be 1 second).
*/
static void UpdateGameList()
{
    if (m_joinGamesState.refreshing) {
        if (g_currentTick >= m_lastRequestTick + REFRESH_INTERVAL) {
            do {
                m_lastRequestTick += REFRESH_INTERVAL;
            } while (g_currentTick >= m_lastRequestTick + REFRESH_INTERVAL);
            SendListGamesRequest();
        }
        auto timeout = m_findGamesTimeout ? m_findGamesTimeout : GetNetworkTimeout();
        if (g_currentTick >= m_startSearchTick + timeout) {
            m_lastRequestTick = 0;
            m_joinGamesState.refreshing = false;
        }
    }
}

/** CheckForWaitingGameReply

    Get a packet from the network and see if it's a game info packet. If so, check
    if it's a new game and add it to our internal list.
*/
static void CheckForWaitingGameReply()
{
    assert(m_state == ST_WAITING_TO_JOIN);
    word type;
    char *packet, respondingGameName[GAME_NAME_LENGTH + 1], *p;
    int length;
    IPX_Address node;

    if ((packet = ReceivePacket(&length, &node))) {
        type = GetRequestType(packet, length);
        /* we're only interested in game info data here */
        if (type == PT_GAME_INFO_DATA) {
            int networkVersion, networkSubversion, numPlayers, maxPlayers;
            byte gameId;

            /* danger Will Robinson, stack reserves are burning out */
            if (UnpackGameInfoPacket(packet, length, &networkVersion, &networkSubversion,
                &numPlayers, &maxPlayers, respondingGameName, GAME_NAME_LENGTH, &gameId)) {
                WriteToLog("Game response received: %s, id = %d, players: %d/%d", respondingGameName, gameId, numPlayers, maxPlayers);
                assert(numPlayers > 0 && numPlayers < 10 && maxPlayers > 0 && maxPlayers < 10);

                /* force them to be single-digit as that much extra space we allocated in game name display buffer */
                numPlayers = max(numPlayers, 0);
                numPlayers = min(numPlayers, 9);
                maxPlayers = max(maxPlayers, 0);
                maxPlayers = min(maxPlayers, 9);

                if (networkVersion > NETWORK_VERSION) {
                    WriteToLog("Ignoring game with too new server (%d.%d)", networkVersion, networkSubversion);
                    qFree(packet);
                    return;
                }

                /* check if we already have this game */
                int i;
                for (i = 0; i < m_numWaitingGames; i++)
                    if (AddressMatch(&m_waitingGames[i].address, &node))
                        break;

                if (i < MAX_GAMES_WAITING) {
                    /* add a new game/update old */
                    m_waitingGames[i].id = gameId;
                    /* this is why we need additional space in waiting game name */
                    p = strcpy(strcpy(strcpy(strcpy(m_waitingGames[i].name, respondingGameName),
                        " ("), int2str(numPlayers)), "/");
                    strcpy(strcpy(p, int2str(maxPlayers)), ")");
                    if (i >= m_numWaitingGames) {
                        CopyAddress(&m_waitingGames[i].address, &node);
                        m_numWaitingGames++;
                    }
                }
            } else
                WriteToLog("Rejecting malformed game info packet");
        }
        qFree(packet);
    }
}

/** ClientJoinsGame

    in:
        lobbyState  -> state variables to apply to the lobby/upcoming game
        ourTeamName -> obvious :)

    Called when client is joining the lobby. Does not return until the game is finished.
    Might not return at all in case game is interrupted.
*/
static void ClientJoinsGame(LobbyState *lobbyState, const char *ourTeamName)
{
    assert(lobbyState->numPlayers < MAX_PLAYERS);
    NetPlayer *n = m_connectedPlayers + lobbyState->numPlayers;

    m_numConnectedPlayers = lobbyState->numPlayers;
    for (int i = 0; i < m_numConnectedPlayers; i++)
        m_connectedPlayers[i].flags = lobbyState->playerFlags[i];

    m_numConnectedPlayers = ++lobbyState->numPlayers;
    m_ourPlayerIndex = m_numConnectedPlayers - 1;
    n->flags = 0;

    memcpy(&n->team, &m_connectedPlayers[0].team, sizeof(TeamFile));
    strcpy(n->name, GetPlayerNick());
    strcpy(n->team.name, ourTeamName);

    /* save client options so they don't get overwritten by server options */
    SaveClientMPOptions();
    ApplyMPOptions(SetMPOptions(&lobbyState->options));

    qFree(n->userTactics);
    n->userTactics = nullptr;

    m_joinGameModalFunction(1, nullptr);
    WriteToLog("We joined!!!");
    m_joinId++;     /* we can't use this id anymore */
    m_inLobby = true;

    m_enterGameLobbyFunction();
}

static bool32 JoiningGameOnIdle(LobbyState *lobbyState)
{
    char *packet, ourTeamName[MAX_TEAM_NAME_LEN];
    int packetSize;
    IPX_Address sender, *addresses[MAX_PLAYERS];
    byte errorCode;
    bool result;

    IPX_OnIdle();
    assert(m_joinGameModalFunction);

    if (m_connectionError) {
        if (m_joinGameModalFunction(-1, nullptr)) {
            EnterWaitingToJoinState();
            return true;
        } else
            return false;
    }

    if ((packet = ReceivePacket(&packetSize, &sender)) && AddressMatch(&sender, &m_waitingGames[m_joiningGame].address)) {
        /* server response to our join game attempt has arrived */
        switch (GetRequestType(packet, packetSize)) {
        case PT_JOIN_GAME_REFUSED:
            WriteToLog("Server not allowing us to join.");
            if (!UnpackRefuseJoinGamePacket(packet, packetSize, &errorCode)) {
                WriteToLog("Malformed game join refused packet received.");
                errorCode = 0;
            }

            errorCode = errorCode > sizeofarray(joinGameErrors) ? 0 : errorCode;
            WriteToLog("Error: '%s'", joinGameErrors[errorCode]);
            m_connectionError = true;

            if (result = m_joinGameModalFunction(-1, joinGameErrors[errorCode]))
                EnterWaitingToJoinState();

            qFree(packet);
            return result;

        case PT_JOIN_GAME_ACCEPTED:
            for (int i = 0; i < MAX_PLAYERS; i++)
                addresses[i] = &m_connectedPlayers[i].address;

            *strncpy(ourTeamName, m_connectedPlayers[0].team.name, MAX_TEAM_NAME_LEN) = '\0';

            if (UnpackJoinedGameOkPacket(packet, packetSize, InitLobbyState(lobbyState), addresses)) {
                qFree(packet);
                ClientJoinsGame(lobbyState, ourTeamName);
                return true;
            } else {
                WriteToLog("Rejecting malformed joined game packet.");
            }
            break;

        default:
            /* ignore */
            WriteToLog("Ignoring packet (%#x) while waiting for join game result.", *(word *)packet);
            break;
        }
        qFree(packet);
    }

    assert(m_joinGameModalFunction);
    assert(m_enterGameLobbyFunction);
    /* parameter to modal func: -1 = done, error, 0 = not done, 1 = done, OK
       and it returns true if user wants to move on, false otherwise */
    if (g_currentTick > m_lastRequestTick + GetNetworkTimeout()) {
        /* still no response from server, abort */
        if (m_joinGameModalFunction(-1, "No response from the server.")) {
            EnterWaitingToJoinState();
            return true;
        }
        m_connectionError = true;
    } else {
        /* resend, but be careful not to flood */
        static word lastResendTick;
        if (g_currentTick - lastResendTick > 25) {
            packet = CreateJoinGamePacket(&packetSize, m_joinId, m_waitingGames[m_joiningGame].id,
                GetPlayerNick(), m_connectedPlayers[m_ourPlayerIndex].team.name);
            SendSimplePacket(&m_waitingGames[m_joiningGame].address, packet, packetSize);
            lastResendTick = g_currentTick;
        }
        m_joinGameModalFunction(0, nullptr);
    }

    return false;
}

/***

   Sync state and setting up teams.
   ================================

***/

static void ResetSyncFlags()
{
    for (int i = 0; i < MAX_PLAYERS; i++)
        m_connectedPlayers[i].setUnsynced();
}

static void EnterSyncingState(void (*modalSyncFunction)(), ShowTeamsMenuFunction showTeamsMenuFunction)
{
    WriteToLog("Going into syncing state...");

    SwitchToState(ST_SYNC);
    m_modalSyncFunction = modalSyncFunction;
    m_showTeamsMenuFunction = showTeamsMenuFunction;
    m_lastRequestTick = g_currentTick;      /* reuse this */
    m_syncDone = false;

    ResetSyncFlags();

    if (isPlayer(m_ourPlayerIndex)) {
        if (!m_connectedPlayers[m_ourPlayerIndex].userTactics)
            m_connectedPlayers[m_ourPlayerIndex].userTactics = (Tactics *)qAlloc(TACTIC_SIZE * 6);
        assert(m_connectedPlayers[m_ourPlayerIndex].userTactics);
        memcpy(m_connectedPlayers[m_ourPlayerIndex].userTactics, GetUserMpTactics(), sizeof(Tactics) * 6);
    }

    if (m_weAreTheServer && isPlayer(0)) {
        int i, length;
        char *packet = CreateTeamAndTacticsPacket(&length, &m_connectedPlayers[0].team, GetUserMpTactics());
        for (i = 1; i < m_numConnectedPlayers; i++) {
            // maybe some resend flag if it fails?
            SendImportantPacket(&m_connectedPlayers[i].address, packet, length);
        }
    }
}

/** ControllingPlayerTimedOut

    i - index of a player that timed out

    We lost a player, game is ruined. If that player was also the server we go
    back to join game menu, otherwise try to go back to lobby.
*/
void ControllingPlayerTimedOut(int i)
{
    if (i < 0)
        for (i = 0; i < m_numConnectedPlayers; i++)
            if (isControlling(i))
                break;

    assert(i < m_numConnectedPlayers && i != m_ourPlayerIndex && isPlayer(i));
    WriteToLog("We lost a player, index %d, name %s", i, m_connectedPlayers[i].getName());

    bool serverDown = i == 0;
    if (serverDown) {
        /* we lost the server */
        DisbandGame();
    } else {
        DisconnectClient(i, false);
        if (m_weAreTheServer) {
            HandleChatLine(-1, "LAUNCH FAILED.", 10);
            SwitchToState(ST_WAITING_TO_START);
        } else {
            SwitchToState(ST_GAME_LOBBY);
        }

        CancelSendingPackets();
    }

    assert(m_setupTeamsOnErrorFunction);
    m_setupTeamsOnErrorFunction(serverDown);
}

/* =============== */

/** HandleTimeouts

    Keep the unacknowledged important packets running, and if timeout is detected
    handle it depending on current state we're in.
*/
static bool HandleTimeouts()
{
    if (UnAckPacket *timedOutPacket = ResendUnacknowledgedPackets()) {
        WriteToLog("Fatal network error, timed-out important packet [%#.12s], ticks = %hd, current ticks = %hd.",
            &timedOutPacket->address, timedOutPacket->time, g_currentTick);
        HexDumpToLog(timedOutPacket->data, timedOutPacket->size, "timed out packet");

        /* network error */
        switch (m_state) {
        case ST_WAITING_TO_START:
            /* client should never be in this state */
            assert(m_weAreTheServer);
            int i;
            for (i = 1; i < m_numConnectedPlayers; i++)
                if (AddressMatch(&m_connectedPlayers[i].address, &timedOutPacket->address))
                    break;

            if (i >= m_numConnectedPlayers)
                WriteToLog("Double disconnect");
            else {
                WriteToLog("Disconnecting player: '%s' (timeout)", m_connectedPlayers[i].getName());
                DisconnectClient(i, true);
                char buf[32];
                FormatPlayerLeftMessage(buf, i);
                HandleChatLine(-1, buf, 10);
            }
            break;

        case ST_GAME_LOBBY:
            /* client-only state */
            assert(!m_weAreTheServer);
            /* server should be our only connection */
            DisconnectClient(0, true);
            SwitchToState(ST_NONE);
            if (m_disconnectedFunction)
                m_disconnectedFunction();
            IPX_OnIdle();
            return false;

        case ST_SYNC:
            /* we shouldn't get here */
            assert(0);
            DisbandGame();
            break;

        case ST_PL1_SETUP_TEAM:
        case ST_PL2_SETUP_TEAM:
        case ST_CONFIRMING_KEYS:
            {
                int i = FindPlayer(&timedOutPacket->address);
                if (i < m_numConnectedPlayers) {
                    assert(i != m_ourPlayerIndex);

                    /* if it's the other player, then we have a problem */
                    if (isPlayer(i)) {
                        ControllingPlayerTimedOut(i);
                    } else {
                        /* if it's a watcher we don't care, just disconnect them so no more packets get sent to them */
                        DisconnectClient(i, false);
                    }
                }
            }
            break;

        default:
            WriteToLog("This state %s doesn't know how to handle timed out packets.", StateToString(m_state));
            break;
        }
    }

    return true;
}

/** NetworkOnIdle

    Network loop during menus. Return false during blocking operations.
    That will cause menu not to be drawn this frame.
*/
bool32 NetworkOnIdle()
{
    bool result = true;

    switch (m_state) {
    case ST_WAITING_TO_JOIN:
        UpdateGameList();
        CheckForWaitingGameReply();
        SendWaitingGamesReport();
        break;

    case ST_WAITING_TO_START:
        CheckIncomingServerLobbyTraffic();
        SendPingPackets();
        /* form state and send it to GUI to update */
        assert(m_updateLobbyFunction && m_weAreTheServer);
        m_updateLobbyFunction(PutOurPlayerOnTop(InitLobbyState(m_lobbyState)));
        break;

    case ST_TRYING_TO_JOIN_GAME:
        return JoiningGameOnIdle(m_lobbyState);

    case ST_GAME_LOBBY:
        if (!CheckIncomingClientLobbyTraffic())
            return false;

        PingServer();
        assert(m_updateLobbyFunction && !m_weAreTheServer);
        m_updateLobbyFunction(PutOurPlayerOnTop(InitLobbyState(m_lobbyState)));
        break;

    case ST_SYNC:
        return SyncOnIdle();

    case ST_PL1_SETUP_TEAM:
    case ST_PL2_SETUP_TEAM:
        HandleNetworkKeys();
        break;

    case ST_CONFIRMING_KEYS:
        if (!WaitForKeysConfirmation(m_setupTeamsModalFunction))
            result = false;
        break;

    case ST_NONE:
        /* do nothing */
        return true;

    case ST_GAME:
        /* just send final acknowledgment packets while stadium menu is displayed */
        DismissIncomingPackets();
        return true;

    default:
        WriteToLog("Unknown state: %s", StateToString(m_state));
    }

    if (!HandleTimeouts())
        return false;

    if (m_state != ST_NONE)
        IPX_OnIdle();

    return result;
}
