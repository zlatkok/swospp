/*
    Multiplayer module - hellz yeah!
*/

#include "types.h"
#include "dos.h"
#include "util.h"
#include "mplayer.h"
#include "dosipx.h"
#include "qalloc.h"
#include "mppacket.h"
#include "options.h"

extern void ApplyOptions(const MP_Options *options);
#pragma aux ApplyOptions "*" parm [edi] modify nomemory exact [eax];

static char chatLines[MAX_CHAT_LINES][MAX_CHAT_LINE_LENGTH + 1];
static byte chatLineColors[MAX_CHAT_LINES];
static int currentChatLine;

char playerNick[NICKNAME_LEN + 1];  /* player name for online games         */
char gameName[GAME_NAME_LENGTH];    /* name of current game                 */
dword currentTeamId = -1;           /* id of current team                   */

static WaitingToJoinReport joinGamesState;
static MP_Options mpOptions;
static MP_Options *savedClientOptions;
static LobbyState *lobbyState;
static byte weAreTheServer; /* WE ARE THE ROBOTS */
static byte connectionError;
static byte joinId;
static byte gameId;
static byte syncDone;
static byte inLobby;

/* GUI callbacks */
static SendWaitingToJoinReport waitingToJoinReportFunc;
static bool (*modalFunction)(int status, const char *errorText);
static void (*enterGameLobbyFunction)();
static void (*updateLobbyFunction)(const LobbyState *state);
static void (*disconnectedFunction)();
static bool (*modalSyncFunction)();
static void (*showTeamsMenuFunction)(const char *, const char *);
static void (*onErrorFunction)();       /* called when game exits due to error */
static void (*onGameEndFunction)();     /* called when game ends naturally */

/* local info: addresses, names, ids and number of games collected so far */
typedef struct WaitingGame {
    IPX_Address address;
    char name[GAME_NAME_LENGTH + 1];
    byte id;
} WaitingGame;

static int numWaitingGames;
static WaitingGame waitingGames[MAX_GAMES_WAITING];
static int joiningGame; /* index of a game we're currently trying to join */

enum MP_States {
    ST_NONE = 0x1123,
    ST_WAITING_TO_JOIN,
    ST_WAITING_TO_START,        /* state for server */
    ST_TRYING_TO_JOIN_GAME,
    ST_GAME_LOBBY,              /* state for client */
    ST_SYNC,
    ST_PL1_SETUP_TEAM,
    ST_PL2_SETUP_TEAM,
    ST_GAME
};

#ifdef DEBUG
const char *stateToString(enum MP_States state)
{
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

static enum MP_States state = ST_NONE;
static word lastRequestTick;
static word startSearchTick;
static word lastPingTime;

typedef struct NetPlayer {
    char name[NICKNAME_LEN + 1];
    char team[TEAM_SIZE];
    Tactics *userTactics;
    IPX_Address address;
    byte flags;         /* bit 0 - is watcher
                           bit 1 - is ready
                           bit 2 - is synced
                           bit 3 - is player in hold of menu controls */
    word joinId;
    word lastPingTime;
} NetPlayer;

enum PlayerFlags {
    PL_IS_WATCHER       = 1,
    PL_IS_READY         = 2,
    PL_IS_SYNCED        = 4,
    PL_IS_CONTROLLING   = 8
};

#define isPlayer(i)         (!(connectedPlayers[i].flags & PL_IS_WATCHER))
#define isReady(i)          (connectedPlayers[i].flags & PL_IS_READY)
#define isSynced(i)         (connectedPlayers[i].flags & PL_IS_SYNCED)
#define isControlling(i)    (connectedPlayers[i].flags & PL_IS_CONTROLLING)

static NetPlayer connectedPlayers[MAX_PLAYERS];
static int numConnectedPlayers;
static int ourPlayerIndex;

static bool LoadTeam(dword teamId, char *dest);
static LobbyState *initLobbyState(LobbyState *state);
static LobbyState *putOurPlayerOnTop(LobbyState *state);
static void CleanUpPlayers();

static void EnterSyncingState(bool (*modalSyncFunc)(int), void (*showTeamMenuFunc)(const char *, const char *));
static void DisconnectClient(int playerIndex, bool sendDisconnect);
static void SendListGamesRequest();
static void SendWaitingGamesReport();

static void registerMPOptions(RegisterOptionsFunc registerOptions);

/** RegisterNetworkOptions

    registerOptions -> callback to register multiplayer options to be saved/loaded.
*/
void RegisterNetworkOptions(RegisterOptionsFunc registerOptions)
{
    registerOptions("multiplayer", 11, "Options for multiplayer games", 29,
        "%*s/playerNick%*s/gameName%4d/team", sizeof(playerNick), playerNick,
        sizeof(gameName), gameName, &currentTeamId);
    registerMPOptions(registerOptions);
}

/* keep track of commands received from the network */
static dword networkControlQueue[KEY_BUFFER_SIZE];
static signed char numCommands;

void BroadcastControls(byte controls, word longFireFlag)
{
    static byte lastControls;
    int i, length;
    char *packet;
    if (!isControlling(ourPlayerIndex) || !controls && !lastControls)
        return;
    lastControls = controls;
    packet = createControlsPacket(&length, controls, longFireFlag);
    for (i = 0; i < numConnectedPlayers; i++)
        if (i != ourPlayerIndex)
            SendImportantPacket(&connectedPlayers[i].address, packet, length);
}

void AddControlsToNetworkQueue(byte controls, word longFireFlag)
{
    if (isControlling(ourPlayerIndex))
        return;
    if (numCommands < sizeof(networkControlQueue))
        networkControlQueue[numCommands++] = controls | longFireFlag << 16;
    else
        WriteToLog(("Command buffer full. Lost a command."));
}

/* Return -1 to signal we should get real key. */
dword GetControlsFromNetwork()
{
    if (isControlling(ourPlayerIndex))
        return -1;
    return numCommands - 1 >= 0 ? networkControlQueue[--numCommands] : 0;
}

bool SwitchToNextControllingState()
{
    if (state == ST_PL1_SETUP_TEAM) {
        int i;
        WriteToLogM((LM_SYNC, "Player 2 now setting up the team."));
        state = ST_PL2_SETUP_TEAM;
        numCommands = 0;        /* flush queue, just in case */
        /* player 1 finished, pass on control to player 2 */
        for (i = 0; i < numConnectedPlayers; i++) {
            if (isPlayer(i))
                if (isControlling(i)) {
                    WriteToLog(("Player %d not controlling anymore.", i));
                    connectedPlayers[i].flags &= ~PL_IS_CONTROLLING;
                } else {
                    connectedPlayers[i].flags |= PL_IS_CONTROLLING;
                    WriteToLog(("Player %d now in control.", i));
                    break;
                }
        }
        assert(i < numConnectedPlayers);
        /* make current controlling player tactics show in all clients */
        memcpy(USER_A, connectedPlayers[i].userTactics, sizeof(Tactics) * 6);
    } else if (state == ST_PL2_SETUP_TEAM) {
        /* game about to go! */
        int i, playerNo = 0, numWatchers = 0, playerIndex = 0;
        IPX_Address *addresses;
        IPX_Address *watcherAddresses[MAX_PLAYERS - 2];
        IPX_Address *playerAddresses[2];
        Tactics *pl1CustomTactics = nullptr, *pl2CustomTactics = nullptr;
        /* player 2 finished, starting the game! */
        WriteToLog(("Game is starting!"));
        state = ST_GAME;
        for (i = 0; i < numConnectedPlayers; i++) {
            if (isPlayer(i)) {
                playerAddresses[playerIndex++] = &connectedPlayers[i].address;
                assert(playerIndex <= 2);
                if (i == ourPlayerIndex)
                    playerNo = playerIndex;
                *(!pl1CustomTactics ? &pl1CustomTactics : &pl2CustomTactics) = connectedPlayers[i].userTactics;
            } else
                watcherAddresses[numWatchers++] = &connectedPlayers[i].address;
            connectedPlayers[i].flags &= ~PL_IS_SYNCED; /* clear this in case it left from previous game */
        }
        addresses = qAlloc((numWatchers + 2) * sizeof(IPX_Address));
        assert(addresses);
        memcpy(addresses, playerAddresses[0], sizeof(IPX_Address));
        memcpy(addresses + 1, playerAddresses[1], sizeof(IPX_Address));
        for (i = 0; i < numWatchers; i++)
            memcpy(addresses + i + 2, watcherAddresses[i], sizeof(IPX_Address));
        assert(pl1CustomTactics && pl2CustomTactics);
        InitMultiplayerGame(playerNo, addresses, numWatchers, addresses + 2, pl1CustomTactics, pl2CustomTactics);
        return true;
    } else
        WriteToLog(("Invalid state %#x!!!", state));
    return false;
}

/* Team setting up is canceled, go back to lobby. */
void GoBackToLobby()
{
    WriteToLog(("Exit pressed, going back to lobby."));
    state = weAreTheServer ? ST_WAITING_TO_START : ST_GAME_LOBBY;
}

void InitMultiplayer()
{
    lobbyState = qAlloc(sizeof(LobbyState));
    assert(lobbyState);
    connectionError = false;
    mpOptions.size = sizeof(mpOptions);
}

void FinishMultiplayer()
{
    qFree(lobbyState);
    lobbyState = nullptr;
    qFree(savedClientOptions);
    savedClientOptions = nullptr;
    CleanUpPlayers();
}

/* Generate some funny random nicknames for starters, if nothing saved. */
void InitPlayerNick()
{
    /* take care of max length when adding here */
    static const char *defaultNicks[] = {
        "PLAYER",
        "MARINE",
        "CYBORG",
        "PREACHER",
        "SOLDIER",
        "KILLER",
        "FAN",
        "FANATIC",
        "MANIAC",
        "PSYCHO",
        "MACHINE",
        "BOT",
        "OFFICER",
        "GUARDIAN",
        "KING",
        "CHAMPION",
        "KNIGHT",
        "FIGHTER",
        "DRAGON",
        "WITNESS",
        "WANDERER",
        "HIGHLANDER",
        "APPRENTICE",
        "MASTER",
        "PRACTITIONER",
        "PILOT",
        "MONSTER",
        "FALCON",
        "HAMMER",
        "MONK",
        "WIZARD"
    };
    if (!playerNick[0]) {
        int index;
        char *p;
        calla(Rand);
        index = (dword)D0 * (sizeof(defaultNicks) / sizeof(defaultNicks[0]) - 1) / 255;
        WriteToLog(("Random index = %d", index));
        p = strcpy(strcpy(playerNick, "SWOS "), defaultNicks[index]);
        calla(Rand);
        strcpy(strcpy(p, " "), int2str(D0));
        WriteToLog(("Random name generated: %s", playerNick));
    }
}

#pragma disable_message(200);
void InitGameName()
{
    if (!gameName[0]) {
        unsigned short year;
        unsigned char month, day, hour, minute, second;
        char *p;
        /* generate random game name from date + time */
        _asm {
            mov  ah, 0x2a
            int  0x21       ; DOS Get System Date
            mov  year, cx
            mov  month, dh
            mov  day, dl
            mov  ah, 0x2c
            int  0x21       ; DOS Get System Time
            mov  hour, ch
            mov  minute, cl
            mov  second, dh
        };
        /* Watcom is optimizing following call by calling int2str successively,
           which fails because it's returning static buffer:
           strcpy(strcpy(strcpy(strcpy(strcpy(strcpy(gameName, "SWPP-"),
               int2str(year)), "-"), int2str(month)), "-"), int2str(day)); */
        p = strcpy(strcpy(strcpy(gameName, "SWPP-"), int2str(year)), "-");
        p = strcpy(strcpy(p, int2str(month)), "-");
        p = strcpy(strcpy(p, int2str(day)), "-");
        p = strcpy(strcpy(p, int2str(hour)), ":");
        p = strcpy(strcpy(p, int2str(minute)), ":");
        p = strcpy(p, int2str(second));
        WriteToLog(("Generated game name: %s", gameName));
    }
    inLobby = false;
}
#pragma enable_message(200);

static void CleanUpPlayers()
{
    int i;
    for (i = 0; i < numConnectedPlayers; i++) {
        qFree(connectedPlayers[i].userTactics);
        connectedPlayers[i].userTactics = nullptr;
    }
    numConnectedPlayers = 0;
}

void DisbandGame()
{
    int i, length;
    char *packet = createPlayerLeftPacket(&length, 0);
    if (weAreTheServer) {
        WriteToLog(("Game disbanded."));
        for (i = 1; i < numConnectedPlayers; i++) {
            setPlayerIndex(packet, i);
            SendSimplePacket(&connectedPlayers[i].address, packet, length);
            DisconnectFrom(&connectedPlayers[i].address);
        }
    } else {
        DisconnectClient(0, true);
        WriteToLog(("Game left."));
    }
    state = ST_NONE;
    CancelSendingPackets();
    qFree(savedClientOptions);
    savedClientOptions = nullptr;
    CleanUpPlayers();
    inLobby = false;
}

#pragma aux aDataTeam_nnn "*";
#pragma aux aCustoms_edt "*";

/* LoadTeam

   teamId -  id of team to load
   dest   -> pointer to buffer that will receive loaded team, if everything ok

   Load specified team into memory, do some basic check if id is valid. Return
   true if everything went well, false for error.
*/
static bool LoadTeam(dword teamId, char *dest)
{
    extern char aDataTeam_nnn[], aCustoms_edt[];
    int teamNo = teamId & 0xff, ordinal = teamId >> 8 & 0xff, handle;
    char *fileName = aDataTeam_nnn;
    if (teamId == -1)
        return false;
    WriteToLog(("Trying to load team %#x, teamNo = %d, ordinal = %d", teamId, teamNo, ordinal));
    if (ordinal > 93) {
        WriteToLog(("Too big ordinal. Rejecting team id %#x", teamId));
        return false;
    }
    if (teamNo == 100)
        fileName = aCustoms_edt;
    else {
        aDataTeam_nnn[10] = teamNo / 100 + '0';
        aDataTeam_nnn[11] = teamNo / 10 % 10 + '0';
        aDataTeam_nnn[12] = teamNo % 10 + '0';
    }
    if ((handle = OpenFile(F_READ_ONLY, fileName)) == -1) {
        WriteToLog(("Team id %#x invalid (team file missing).", teamId));
        return false;
    }
    CloseFile(handle);
    D0 = teamNo;
    calla(LoadTeamFile);
    memcpy(dest, teamFileBuffer + 2 + ordinal * TEAM_SIZE, TEAM_SIZE);
    dest[4] = 2;                                /* controlled by player */
    *strncpy(dest + 36, playerNick, 22) = '\0'; /* set coach name to player nickname */
    WriteToLog(("Loaded team '%s' OK.", dest + 5));
    return true;
}


/* This is an important thing to do, otherwise play match menu will be working with teams in wrong
   locations, and will fail to write changes properly. So make it look like as if team was loaded from
   friendly game team selection. */
static char *AddTeamToSelectedTeams(const char *team)
{
    return (char *)memcpy(selectedTeamsBuffer + TEAM_SIZE * numSelectedTeams++, team, TEAM_SIZE) - TEAM_SIZE;
}

/* Return player index from ipx address. */
static int FindPlayer(const IPX_Address *node)
{
    int i;
    for (i = 0; i < numConnectedPlayers; i++)
        if (addressMatch(node, &connectedPlayers[i].address))
            break;
    return i;
}

static void InitPlayer(NetPlayer *player, const char *name, const char *teamName,
    const IPX_Address *address, byte joinId)
{
    player->name[0] = '\0';
    player->team[0] = '\0';
    if (name)
        *strncpy(player->name, name, NICKNAME_LEN) = '\0';
    if (teamName)
        *strncpy(player->team + 5, teamName, TEAM_SIZE) = '\0';
    *(dword *)player->team = -1;
    player->userTactics = nullptr;
    if (address)
        copyAddress(&player->address, address);
    player->flags = 0;
    player->joinId = joinId;
    player->lastPingTime = 0;
}

/* Add a chat line to our storage and dispatch to clients if we're the server. */
static void HandleChatLine(int senderIndex, const char *text, byte color)
{
    static int lastSender = -1;
    int startLine = 0, endLine = 0;
    byte colors[2];
    char *linesToSend[2];
    currentChatLine = (currentChatLine + 1) % MAX_CHAT_LINES;
    colors[0] = colors[1] = color;
    linesToSend[0] = linesToSend[1] = chatLines[currentChatLine];
    if (weAreTheServer && senderIndex >= 0 && senderIndex != lastSender) {
        char *p = strncpy(chatLines[currentChatLine],
            connectedPlayers[senderIndex].name, MAX_CHAT_LINE_LENGTH);
        if (p - chatLines[currentChatLine] < MAX_CHAT_LINE_LENGTH)
            *p++ = ':';
        *p = '\0';
        chatLineColors[currentChatLine] = colors[0] = 13;
        lastSender = senderIndex;
        currentChatLine = (currentChatLine + 1) % MAX_CHAT_LINES;
        linesToSend[1] = chatLines[currentChatLine];
        endLine++;
        color = 2;
    }
    *strncpy(chatLines[currentChatLine], text, MAX_CHAT_LINE_LENGTH) = '\0';
    chatLineColors[currentChatLine] = color;
    /* Dispatch chat line(s) to clients. */
    if (weAreTheServer) {
        int i, j, length;
        for (i = startLine; i <= endLine; i++) {
            char *packet = createPlayerChatPacket(&length, linesToSend[i], colors[i]);
            for (j = 1; j < numConnectedPlayers; j++)
                SendImportantPacket(&connectedPlayers[j].address, packet, length);
        }
    }
}

static void DisconnectClient(int playerIndex, bool sendDisconnect)
{
    int i, length;
    char *packet = createPlayerLeftPacket(&length, playerIndex);
    assert(numConnectedPlayers < MAX_PLAYERS);
    if (playerIndex < 0 || playerIndex >= numConnectedPlayers) {
        WriteToLog(("Invalid player index in disconnect (%d)."));
        return;
    }
    /* send diconnect packet to offending client, but don't wait for reply
       (connection most likely already lost anyway) */
    if (sendDisconnect)
        SendSimplePacket(&connectedPlayers[playerIndex].address, packet, length);
    DisconnectFrom(&connectedPlayers[playerIndex].address);
    qFree(connectedPlayers[playerIndex].userTactics);
    connectedPlayers[playerIndex].userTactics = nullptr;
    if (weAreTheServer) {
        for (i = 1; i < numConnectedPlayers; i++)
            if (i != playerIndex && i != playerIndex)
                SendImportantPacket(&connectedPlayers[i].address, packet, length);
        /* shift everything up */
        memmove(connectedPlayers + playerIndex, connectedPlayers + playerIndex + 1,
            sizeof(connectedPlayers[0]) * (numConnectedPlayers-- - playerIndex - 1));
    }
}

static void ServerHandlePlayerLeftPacket(const char *packet, int length, const IPX_Address *node)
{
    int playerIndex;
    char buf[32];
    if (!unpackPlayerLeftPacket(packet, length, &playerIndex)) {
        WriteToLog(("Rejecting invalid player disconnect packet."));
        return;
    }
    if ((playerIndex = FindPlayer(node)) > numConnectedPlayers - 1) {
        WriteToLog(("Probably left-over disconnect request received. Ignoring."));
        return;
    }
    WriteToLog(("About to disconnect player (by their request), playerIndex = %d, numConnectedPlayers = %d",
        playerIndex, numConnectedPlayers));
    strcpy(strupr(strcpy(buf, connectedPlayers[playerIndex].name)), " LEFT.");
    DisconnectClient(playerIndex, true);
    HandleChatLine(-1, buf, 10);
}

static bool ClientHandlePlayerLeftPacket(const char *packet, int length, const IPX_Address *node)
{
    int playerIndex;
    if (!unpackPlayerLeftPacket(packet, length, &playerIndex)) {
        WriteToLog(("Malformed player left packet rejected."));
    } else {
        if (!addressMatch(node, &connectedPlayers[0].address)) {
            WriteToLog(("Client sent us PT_PLAYER_LEFT [%#.12s]", node));
            return true;
        }
        /* "Don’t ask for whom the bell tolls, it tolls for thee..." */
        if (playerIndex == ourPlayerIndex) {
            WriteToLog(("Server has disconnected us."));
            DisconnectClient(0, true);
            state = ST_NONE;
            assert(disconnectedFunction);
            disconnectedFunction();
            return false;
        } else {
            if (playerIndex > numConnectedPlayers) {
                WriteToLog(("Invalid player remove request (%d/%d)", playerIndex, numConnectedPlayers));
                return true;
            }
            WriteToLog(("Removing player %d, num. connected players is %d", playerIndex, numConnectedPlayers));
            assert(playerIndex > 0);
            memmove(connectedPlayers + playerIndex, connectedPlayers + playerIndex + 1,
                sizeof(connectedPlayers[0]) * (numConnectedPlayers-- - playerIndex - 1));
            if (ourPlayerIndex >= numConnectedPlayers)
                ourPlayerIndex--;
        }
    }
    return true;
}


/***

   Game Lobby menu GUI interfacing.
   =================================

***/

void CreateNewGame(const MP_Options *options, void (*updateLobbyFunc)(const LobbyState *),
    void (*errorFunc)(), void (*onGameEndFunc)(), bool inWeAreTheServer)
{
    int i;
    WriteToLog(("Creating new IPX game as a %s, state = %#x...", inWeAreTheServer ? "server" : "client", state));
    assert(state == ST_NONE || state == ST_TRYING_TO_JOIN_GAME);
    state = inWeAreTheServer ? ST_WAITING_TO_START : ST_GAME_LOBBY;
    assert(options->size == mpOptions.size  && mpOptions.size == sizeof(mpOptions));
    memcpy(&mpOptions, options, sizeof(mpOptions));
    updateLobbyFunction = updateLobbyFunc;
    onErrorFunction = errorFunc;
    onGameEndFunction = onGameEndFunc;
    weAreTheServer = inWeAreTheServer;
    if (weAreTheServer) {
        numConnectedPlayers = 1;    /* self */
        ourPlayerIndex = 0;
        inLobby = true;
        /* do a full initialization in order not to forget something */
        InitPlayer(&connectedPlayers[0], playerNick, nullptr, nullptr, 0);
        GetOurAddress(&connectedPlayers[0].address);
        /* if we have the team number, load it */
        if (currentTeamId != -1 && !LoadTeam(currentTeamId, connectedPlayers[0].team)) {
            WriteToLog(("Failed to load team %#x.", currentTeamId));
            currentTeamId = -1;
        }
        calla(Rand);
        gameId = D0;
        WriteToLog(("Game ID is %d", gameId));
    }
    for (i = 0; i < MAX_CHAT_LINES; i++) {
        chatLines[i][0] = '\0';
        chatLineColors[i] = 2;
    }
    currentChatLine = -1;
}

static const MP_Options defaultOptions = {DEFAULT_MP_OPTIONS};

void InitializeMPOptions(const MP_Options *newOptions)
{
    assert(newOptions->size <= sizeof(mpOptions));
    /* if new options are bigger we could have an update */
    if (newOptions->size < sizeof(mpOptions))
        mpOptions = defaultOptions;     /* force newly added option fields to their default values */
    memcpy(&mpOptions, newOptions, newOptions->size);
    mpOptions.size = sizeof(mpOptions);
}

/* Return currently active MP options for options manager. */
MP_Options *getMPOptions()
{
    return savedClientOptions ? savedClientOptions : &mpOptions;
}

/* Update this whenever MP_Options get changed! */
void registerMPOptions(RegisterOptionsFunc registerOptions)
{
    assert(sizeof(MP_Options) == 12);
    registerOptions("game", 4, "Default settings for multiplayer match", 38,
        "%n%c" "%2d/version" "%2d/gameLength" "%2d/pitchType" "%1d/numSubs" "%1d/maxSubs"
        "%1d/skipFrames" "%1d/networkTimeout", getMPOptions);
}

void UpdateMPOptions(const MP_Options *newOptions)
{
    char *packet;
    int i, length;
    assert(newOptions->size == mpOptions.size && mpOptions.size == sizeof(mpOptions));
    assert(weAreTheServer);
    if (weAreTheServer && memcmp(&mpOptions, newOptions, sizeof(mpOptions))) {
        packet = createOptionsPacket(&length, newOptions);
        /* signal a change in options (server-only) */
        for (i = 1; i < numConnectedPlayers; i++)
            SendImportantPacket(&connectedPlayers[i].address, packet, length);
    }
    InitializeMPOptions(newOptions);
}

bool CompareMPOptions(const MP_Options *newOptions)
{
    if (!mpOptions.size)
        mpOptions = defaultOptions;
    assert(newOptions->size <= mpOptions.size && mpOptions.size == sizeof(mpOptions));
    return memcmp(savedClientOptions ? savedClientOptions : &mpOptions,
        newOptions, min(newOptions->size, sizeof(mpOptions))) == 0;
}

MP_Options *GetMPOptions(MP_Options *options)
{
    if (!mpOptions.size)
        mpOptions = defaultOptions;
    assert(mpOptions.size == sizeof(mpOptions));
    memcpy(options, savedClientOptions ? savedClientOptions : &mpOptions, sizeof(mpOptions));
    return options;
}

static void SetFlags(int bit, bool set)
{
    char *packet;
    int i, length;
    byte oldFlags = connectedPlayers[ourPlayerIndex].flags;
    connectedPlayers[ourPlayerIndex].flags = oldFlags & ~(1 << bit) | (set != 0) << bit;
    if (oldFlags != connectedPlayers[ourPlayerIndex].flags) {
        packet = createPlayerFlagsPacket(&length, connectedPlayers[ourPlayerIndex].flags, ourPlayerIndex);
        if (weAreTheServer)
            for (i = 1; i < numConnectedPlayers; i++)
                SendImportantPacket(&connectedPlayers[i].address, packet, length);
        else
            SendImportantPacket(&connectedPlayers[0].address, packet, length);
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

char *SetTeam(char *newTeam, dword teamIndex)
{
    char *packet;
    int i, length;
    memcpy(connectedPlayers[ourPlayerIndex].team, newTeam, TEAM_SIZE);
    currentTeamId = teamIndex;
    if (!inLobby)
        return newTeam + 5;
    packet = createPlayerTeamChangePacket(&length, 0, newTeam + 5);
    if (weAreTheServer)
        for (i = 1; i < numConnectedPlayers; i++)
            SendImportantPacket(&connectedPlayers[i].address, packet, length);
    else
        SendImportantPacket(&connectedPlayers[0].address, packet, length);
    return newTeam + 5;
}

/* User has typed a chat line. */
void AddChatLine(const char *line)
{
    if (!line || !*line)
        return;
    if (weAreTheServer) {
        HandleChatLine(0, line, 2);
    } else {
        int length;
        char *packet = createPlayerChatPacket(&length, line, 0);
        SendImportantPacket(&connectedPlayers[0].address, packet, length);
    }
}

/* CanGameStart

   See if conditions are fullfiled for starting a game. Those can be summarized as:
   - having exactly 2 players
   - both players must be ready
   - both players must have their teams selected
*/
bool CanGameStart()
{
    int i, numReadyPlayers = 0, numNonReadyPlayers = 0;
    if (!weAreTheServer)
        return false;
    for (i = 0; i < numConnectedPlayers; i++) {
        if (isPlayer(i)) {
            if (isReady(i) && connectedPlayers[i].team[5] != 0)
                numReadyPlayers++;  /* must be player, ready and have a team */
            else
                numNonReadyPlayers++;
        }
    }
    return numReadyPlayers == 2 && numNonReadyPlayers == 0;
}

void SetupTeams(bool (*modalSyncFunc)(), void (*showTeamMenuFunc)())
{
    char *packet, randomVariables[32];
    int i, length, randVarsSize = sizeof(randomVariables);
    const IPX_Address *addresses[MAX_PLAYERS - 1];
    WriteToLog(("Synchronizing... modalSyncFunc = %#x, showTeamMenuFunc = %#x",
        modalSyncFunc, showTeamMenuFunc));
    for (i = 1; i < numConnectedPlayers; i++)
        addresses[i - 1] = &connectedPlayers[i].address;
    GetRandomVariables(randomVariables, &randVarsSize);
    assert(randVarsSize <= sizeof(randomVariables));
    WriteToLog(("Sending random variables: [%#.*s]", randVarsSize, randomVariables));
    packet = createSyncPacket(&length, addresses, numConnectedPlayers - 1, randomVariables, randVarsSize);
    for (i = 1; i < numConnectedPlayers; i++)
        SendImportantPacket(addresses[i - 1], packet, length);
    EnterSyncingState(modalSyncFunc, showTeamMenuFunc);
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

    /* ignore lost packets due to low mem in hope they'll send again when we have more mem */
    if (!(packet = ReceivePacket(&length, &node)))
        return;
    switch (getRequestType(packet, length)) {
    case PT_GET_GAME_INFO:  /* someone's querying about open games */
        WriteToLog(("Received game info request"));
        calla(Rand);
        gameId = D0;
        response = createGameInfoPacket(&respLength, numConnectedPlayers, gameName, gameId);
        WriteToLog(("Responding to enumerate games request."));
        SendSimplePacket(&node, response, respLength);
        break;
    case PT_JOIN_GAME:      /* someone's trying to join */
        WriteToLog(("Received join game request"));
        if (unpackJoinGamePacket(packet, length, &networkVersion, &networkSubversion,
            &joinId, &receivedGameId, playerName, teamName)) {
            WriteToLog(("Join ID = %d, game ID = %d, our game ID = %d, player name = %s, team name = %s",
                joinId, receivedGameId, gameId, playerName, teamName));
            if (receivedGameId != gameId) {
                WriteToLog(("Invalid game ID, refusing connection."));
                response = createRefuseJoinGamePacket(&respLength, GR_INVALID_GAME);
                SendSimplePacket(&node, response, respLength);
                break;
            }
            if (numConnectedPlayers < MAX_PLAYERS) {
                if (networkVersion >= NETWORK_VERSION) {
                    /* we got a possible new player */
                    NetPlayer *n = connectedPlayers + numConnectedPlayers;
                    const IPX_Address *playerAddresses[MAX_PLAYERS];
                    /* see if we already have them */
                    if ((playerIndex = FindPlayer(&node)) < numConnectedPlayers) {
                        /* there's a possibility that the client crashed but we haven't noticed it,
                           in that case we might wrongfully disallow them to join; so we introduce join
                           id - client will generate one randomly and send it when trying to join, that's
                           how we will be able to differentiate if it's a different join attempt */
                        if (joinId == connectedPlayers[playerIndex].joinId) {
                            WriteToLog(("Ignoring superfluous join request (%s), id = %d",
                                connectedPlayers[playerIndex].name, joinId));
                            qFree(packet);
                            return;
                        } else {
                            /* inform other clients about disconnect, but don't send diconnect to client */
                            WriteToLog(("Re-join detected, old id = %d, new id = %d",
                                connectedPlayers[playerIndex].joinId, joinId));
                            DisconnectClient(playerIndex, false);
                            n = connectedPlayers + numConnectedPlayers;
                        }
                    }
                    InitPlayer(n, playerName, teamName, &node, joinId);
                    WriteToLog(("Joining new player %s to the game, with join id %d.", n->name, n->joinId));
                    ConnectTo(&node);
                    for (i = 0; i < numConnectedPlayers + 1; i++)
                        playerAddresses[i] = &connectedPlayers[i].address;
                    initLobbyState(lobbyState);
                    response = createJoinedGameOkPacket(&respLength, lobbyState, playerAddresses);
                    WriteToLog(("Responding positively to join game request."));
                    SendImportantPacket(&node, response, respLength);
                    /* inform other players that the new one has joined */
                    response = createPlayerJoinedPacket(&respLength, playerName, teamName);
                    for (i = 1; i < numConnectedPlayers; i++)
                        SendImportantPacket(&connectedPlayers[i].address, response, respLength);
                    strcpy(strupr(strcpy(buf, playerName)), " JOINED.");
                    HandleChatLine(-1, buf, 14);
                    numConnectedPlayers++;
                    qFree(packet);
                    return;
                } else {
                    response = createRefuseJoinGamePacket(&respLength, GR_WRONG_VERSION);
                }
            } else {
                response = createRefuseJoinGamePacket(&respLength, GR_ROOM_FULL);
            }
        } else {
            WriteToLog(("Malformed join game packet rejected."));
            break;
        }
        SendSimplePacket(&node, response, respLength);
        break;
    case PT_PLAYER_TEAM:
        if (!unpackPlayerTeamChangePacket(packet, length, &playerIndex, teamName)) {
            WriteToLog(("Invalid team change packet rejected."));
            break;
        }
        if ((playerIndex = FindPlayer(&node)) > numConnectedPlayers) {
            WriteToLog(("Ignoring left-over player team packet."));
            break;
        }
        /* set our internal state */
        *strncpy(connectedPlayers[playerIndex].team + 5, teamName, MAX_TEAM_NAME_LEN) = '\0';
        /* reuse that packet to notify other clients */
        setPlayerIndex(packet, playerIndex);
        for (i = 1; i < numConnectedPlayers; i++)
            if (i != playerIndex)
                SendImportantPacket(&connectedPlayers[i].address, packet, length);
        break;
    case PT_PLAYER_FLAGS:
        if (!unpackPlayerFlagsPacket(packet, length, &playerIndex, &flags)) {
            WriteToLog(("Invalid flags packet rejected."));
            break;
        }
        if ((playerIndex = FindPlayer(&node)) > numConnectedPlayers) {
            WriteToLog(("Ignoring left-over flags packet."));
            break;
        }
        connectedPlayers[playerIndex].flags = flags;
        setPlayerIndex(packet, playerIndex);
        for (i = 1; i < numConnectedPlayers; i++)
            if (i != playerIndex)
                SendImportantPacket(&connectedPlayers[i].address, packet, length);
        break;
    case PT_OPTIONS:
        /* this shouldn't happen (unless rogue client :P) */
        WriteToLog(("Weird, client is sending us options, from address [%#.12s]", &node));
        break;
    case PT_PLAYER_LEFT:
        ServerHandlePlayerLeftPacket(packet, length, &node);
        break;
    case PT_CHAT_LINE:
        //TODO:flood protection
        if (!unpackPlayerChatPacket(packet, length, chatLine, &color)) {
            WriteToLog(("Ignoring invalid chat line packet."));
            break;
        }
        playerIndex = FindPlayer(&node);
        if (playerIndex > 0 && playerIndex < numConnectedPlayers)
            HandleChatLine(playerIndex, chatLine, 2);
        else
            WriteToLog(("Weird, received chat line from a disconnected player."));
        break;
    case PT_PING:
        /* ignore */
        break;
    case PT_GAME_CONTROLS:
        /* possible last packet needing ack */
        SendSimplePacket(&node, packet, length);
        break;
    default:
        WriteToLog(("Unknown packet type received, %#x, length = %d", getRequestType(packet, length), length));
        HexDumpToLog(packet, length, "unknown packet");
    }
    qFree(packet);
}

/* Send ping packet to clients periodically, so we can detect client disconnects even if they're idle. */
static void SendPingPackets()
{
    int i;
    word timeout = GetNetworkTimeout(), currentTime = currentTick, pingPacket = PT_PING;
    for (i = 1; i < numConnectedPlayers; i++) {
        if (currentTime - timeout > connectedPlayers[i].lastPingTime) {
            SendImportantPacket(&connectedPlayers[i].address, (char *)&pingPacket, sizeof(word));
            connectedPlayers[i].lastPingTime = currentTime;
        }
    }
}

static void PingServer()
{
    word timeout = GetNetworkTimeout(), currentTime = currentTick, pingPacket = PT_PING;
    if (currentTime - timeout > lastPingTime) {
        SendImportantPacket(&connectedPlayers[0].address, (char *)&pingPacket, sizeof(word));
        lastPingTime = currentTime;
    }
}

/* Network loop in game lobby for the clients. */
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
    if (!addressMatch(&node, &connectedPlayers[0].address)) {
        WriteToLog(("Rejecting packet probably from other client (non-server): [%#.12s].", &node));
        qFree(packet);
        return true;
    }

    switch (getRequestType(packet, length)) {
    case PT_OPTIONS:
        if (!unpackOptionsPacket(packet, length, &mpOptions))
            WriteToLog(("Malformed options packet rejected."));
        else
            assert(mpOptions.size == sizeof(MP_Options));
        break;
    case PT_PLAYER_FLAGS:
        if (!unpackPlayerFlagsPacket(packet, length, &playerIndex, &flags) || playerIndex >= MAX_PLAYERS)
            WriteToLog(("Malformed flags packet rejected."));
        else
            connectedPlayers[playerIndex].flags = flags;
        assert(playerIndex >= 0);
        break;
    case PT_PLAYER_JOINED:
        if (!unpackPlayerJoinedPacket(packet, length, playerName, playerTeamName)) {
            WriteToLog(("Malformed player joined packet rejected."));
        } else {
            newPlayerIndex = numConnectedPlayers++;
            InitPlayer(connectedPlayers + newPlayerIndex, playerName, playerTeamName, &node, 0x1ff);
            WriteToLog(("New player joined, name: '%s', team name: '%s'", playerName, playerTeamName));
        }
        break;
    case PT_PLAYER_LEFT:
        if (!ClientHandlePlayerLeftPacket(packet, length, &node)) {
            qFree(packet);
            return false;
        }
        break;
    case PT_PLAYER_TEAM:
        if (!unpackPlayerTeamChangePacket(packet, length, &playerIndex, &playerTeamName)) {
            WriteToLog(("Malformed player team change packet rejected."));
        } else {
            assert(playerIndex >= 0 && playerIndex < numConnectedPlayers);
            *strncpy(connectedPlayers[playerIndex].team + 5, playerTeamName, MAX_TEAM_NAME_LEN) = '\0';
        }
        break;
    case PT_PING:
        /* ignore */
        //WriteToLog(("Responded to ping from the server... time is %d", currentTick));
        break;
    case PT_CHAT_LINE:
        if (unpackPlayerChatPacket(packet, length, chatLine, &color)) {
            HandleChatLine(-1, chatLine, color);
        } else
            WriteToLog(("Invalid chat line packet ignored."));
        break;
    case PT_SYNC:
        for (i = 1; i < numConnectedPlayers; i++)
            addresses[i - 1] = &connectedPlayers[i].address;
        /* re-use chat line buffer to hold random variables content */
        i = sizeof(chatLine);
        if (!unpackSyncPacket(packet, length, addresses, numConnectedPlayers - 1, chatLine, &i)) {
            WriteToLog(("Invalid sync packet rejected."));
        } else {
            WriteToLog(("Received random variables: [%#.*s]", i, chatLine));
            /* apply random variables to keep the both clients in perfect sync */
            SetRandomVariables(chatLine, i);
            /* connect to other player(s) to accept their team data */
            for (i = 1; i < numConnectedPlayers; i++)
                if (i != ourPlayerIndex && (isPlayer(i) || isPlayer(ourPlayerIndex)))
                    ConnectTo(&connectedPlayers[i].address);
            /* players send their teams and tactics */
            if (isPlayer(ourPlayerIndex)) {
                char *packet = createTeamAndTacticsPacket(&length, connectedPlayers[ourPlayerIndex].team, USER_A);
                for (i = 0; i < numConnectedPlayers; i++)
                    if (i != ourPlayerIndex) {
                        //mozda neki resend fleg ako ne uspe?
                        SendImportantPacket(&connectedPlayers[i].address, packet, length);
                    }
            }
            /* note that we might reject some packets if they arrive before we connect
               to those clients, but they will resend so everything should be fine */
        }
        EnterSyncingState(modalSyncFunction, showTeamsMenuFunction);
        break;
    case PT_GAME_CONTROLS:
        /* this could be left over last packet needing ack, simply return it to sender */
        SendSimplePacket(&node, packet, length);
        break;
    default:
        WriteToLog(("Unexpected packet %#hx ignored", *(word *)packet));
    }
    qFree(packet);
    return true;
}

static bool GotAllTeamsAndTactics()
{
    int i;
    for (i = 0; i < numConnectedPlayers; i++)
        if (isPlayer(i) && !connectedPlayers[i].userTactics)
            return false;
    return true;
}

/* Multiplayer game just ended. Get the results and go back to corresponding state. */
void GameFinished()
{
    byte gameStatus = GetGameStatus();
    if (gameStatus != GS_GAME_DISCONNECTED && gameStatus != GS_WATCHER_ABORTED) {
        GoBackToLobby();
        onGameEndFunction();
    } else {
        DisbandGame();
        onErrorFunction();
    }
}

static void MenuTransitionAfterTheGame()
{
    /* must refresh menu before the fade in, and after menu conversion */
    if (state == ST_GAME_LOBBY || state == ST_WAITING_TO_START)
        updateLobbyFunction(putOurPlayerOnTop(initLobbyState(lobbyState)));
}

/* SyncOnIdle

   Handle state transition during synchronization - when teams and tactics are
   exchanged among all clients. Server supervises the process and issues final
   result when everything is finished.
*/
static bool SyncOnIdle()
{
    char *packet, *team1 = nullptr, *team2 = nullptr;
    int i, controllingPlayer, packetLen;
    IPX_Address node;
    UnAckPacket *timedOutPacket;
    word timeout = GetNetworkTimeout();
    word syncPacket = PT_SYNC_FAIL;
    word packetType;

    if (packet = ReceivePacket(&packetLen, &node)) {
        switch (packetType = getRequestType(packet, packetLen)) {
        case PT_TEAM_AND_TACTICS:
            if ((i = FindPlayer(&node)) >= numConnectedPlayers)
                WriteToLog(("Received PT_TEAM_AND_TACTICS from a non-connected player [%#.12s].", &node));
            else if (!isPlayer(i))
                WriteToLog(("Received PT_TEAM_AND_TACTICS from a watcher."));
            else {
                qFree(connectedPlayers[i].userTactics);
                connectedPlayers[i].userTactics = qAlloc(sizeof(Tactics) * 6);
                //if !(alloc) FAIL
                if (!unpackTeamAndTacticsPacket(packet, packetLen,
                    connectedPlayers[i].team, connectedPlayers[i].userTactics))
                    WriteToLog(("Malformed PT_TEAM_AND_TACTICS packet received."));
            }
            break;
        case PT_SYNC_OK:
            // check source
            controllingPlayer = -1;
            /* give control to first player, and clear all others */
            for (i = 0; i < numConnectedPlayers; i++)
                if (controllingPlayer < 0 && isPlayer(i)) {
                    connectedPlayers[i].flags |= PL_IS_CONTROLLING;
                    controllingPlayer = i;
                } else
                    connectedPlayers[i].flags &= ~PL_IS_CONTROLLING;
            assert(controllingPlayer >= 0);
            /* make controlling player tactics current */
            memcpy(USER_A, connectedPlayers[controllingPlayer].userTactics, TACTICS_SIZE * 6);
            if (weAreTheServer) {
                if ((i = FindPlayer(&node)) >= numConnectedPlayers)
                    WriteToLog(("Received PT_SYNC_OK from non-connected player."));
                else
                    connectedPlayers[i].flags |= PL_IS_SYNCED;
            } else {
                WriteToLog(("Sync completed successfully."));
                state = ST_PL1_SETUP_TEAM;
                qFree(packet);
                for (i = 0; i < numConnectedPlayers; i++)
                    if (isPlayer(i))
                        if (!team1)
                            team1 = connectedPlayers[i].team;
                        else if (!team2)
                            team2 = connectedPlayers[i].team;
                assert(showTeamsMenuFunction && team1 && team2);
                selTeamsPtr = selectedTeamsBuffer;  /* just in case */
                showTeamsMenuFunction(AddTeamToSelectedTeams(team1), AddTeamToSelectedTeams(team2));
                MenuTransitionAfterTheGame();
                IPX_OnIdle();
                return false;
            }
            break;
        case PT_PLAYER_LEFT:
            // are we server? source?
            if (weAreTheServer)
                ServerHandlePlayerLeftPacket(packet, packetLen, &node);
            else {
                if (!ClientHandlePlayerLeftPacket(packet, packetLen, &node)) {
                    qFree(packet);
                    return false;
                }
            }
            break;
        case PT_SYNC_FAIL:
            if (weAreTheServer) {
                /* someone failed */
                if ((i = FindPlayer(&node)) >= numConnectedPlayers)
                    WriteToLog(("Received PT_SYNC_FAIL from non-connected player."));
                else {
                    /* send fail to everyone and go back to lobby */
                    for (i = 1; i < numConnectedPlayers; i++)
                        SendImportantPacket(&connectedPlayers[i].address, (char *)&syncPacket, sizeof(word));
                    state = ST_WAITING_TO_START;
                    qFree(packet);
                    return true;
                }
            } else if (packetType == PT_SYNC_FAIL) {
                assert(ourPlayerIndex != 0);
                if (addressMatch(&connectedPlayers[0].address, &node)) {
                    /* go back to lobby */
                    state = ST_GAME_LOBBY;
                    qFree(packet);
                    return true;
                } else
                    WriteToLog(("Weird, received PT_SYNC_FAIL from another client."));
            }
            break;
        case PT_PING:
            /* ignore */
            break;
        default:
            WriteToLog(("Ignoring packet %#02x while syncing.", getRequestType(packet, packetLen)));
            break;
        }
    }
    qFree(packet);
    if (!syncDone && GotAllTeamsAndTactics()) {
        if (!weAreTheServer) {
            syncPacket = PT_SYNC_OK;
            SendImportantPacket(&connectedPlayers[0].address, (char *)&syncPacket, sizeof(word));
        } else
            connectedPlayers[0].flags |= PL_IS_SYNCED;
        syncDone = true;
    }
    if (weAreTheServer) {
        for (i = 0; i < numConnectedPlayers; i++)
            if (!isSynced(i))
                break;
        if (i >= numConnectedPlayers) {
            /* notify clients of successful end of operation */
            WriteToLog(("Sync completed successfully. Notifying clients."));
            syncPacket = PT_SYNC_OK;
            if (isPlayer(0))
                team1 = connectedPlayers[0].team;
            for (i = 1; i < numConnectedPlayers; i++) {
                SendImportantPacket(&connectedPlayers[i].address, (char *)&syncPacket, sizeof(word));
                if (isPlayer(i))
                    if (!team1)
                        team1 = connectedPlayers[i].team;
                    else if (!team2)
                        team2 = connectedPlayers[i].team;
            }
            state = ST_PL1_SETUP_TEAM;
            assert(showTeamsMenuFunction && team1 && team2);
            showTeamsMenuFunction(AddTeamToSelectedTeams(team1), AddTeamToSelectedTeams(team2));
            MenuTransitionAfterTheGame();
            IPX_OnIdle();
            return false;
        }
    }
    if (currentTick > lastRequestTick + 3 * timeout / 2) {
        /* time's up */
        if (!weAreTheServer) {
            SendImportantPacket(&connectedPlayers[0].address, (char *)&syncPacket, sizeof(word));
            state = ST_GAME_LOBBY;
        } else {
            /* failure */
            for (i = 1; i < numConnectedPlayers; i++)
                SendImportantPacket(&connectedPlayers[i].address, (char *)&syncPacket, sizeof(word));
            HandleChatLine(-1, "SYNC FAILED: TIMEOUT", 10);
            state = ST_WAITING_TO_START;
        }
        CancelSendingPackets();
        return true;
    }
    if (timedOutPacket = ResendUnacknowledgedPackets()) {
        i = FindPlayer(&timedOutPacket->address);
        if (i >= numConnectedPlayers)
            DisconnectFrom(&timedOutPacket->address);   /* just in case */
        if (weAreTheServer) {
            DisconnectClient(i, true);
        } else {
            WriteToLog(("Timed out packet from [%#.12s]", &timedOutPacket->address));
            if (i == 0) {
                SendSimplePacket(&connectedPlayers[0].address, (char *)&syncPacket, sizeof(word));
                DisconnectClient(0, true);  /* we are disconnected from the server */
                assert(disconnectedFunction);
                CancelSendingPackets();
                state = ST_NONE;
                disconnectedFunction();
                IPX_OnIdle();
                return false;
            } else {
                /* return sync fail to server */
                syncDone = true;
                SendImportantPacket(&connectedPlayers[0].address, (char *)&syncPacket, sizeof(word));
                DisconnectClient(i, true);
            }
        }
    }
    modalSyncFunction();
    IPX_OnIdle();
    return false;
}

/* Create lobby state to send to a connecting player or GUI. */
static LobbyState *initLobbyState(LobbyState *state)
{
    int i, currentOutputLine;
    state->numPlayers = numConnectedPlayers;
    assert(mpOptions.size == sizeof(mpOptions));
    memcpy(&state->options, &mpOptions, sizeof(state->options));
    for (i = 0; i < MAX_PLAYERS; i++) {
        state->playerNames[i] = connectedPlayers[i].name;
        state->playerFlags[i] = connectedPlayers[i].flags;
        state->playerTeamsNames[i] = connectedPlayers[i].team + 5;
    }
    currentOutputLine = (currentChatLine + 1) % MAX_CHAT_LINES;
    for (i = 0; i < MAX_CHAT_LINES; i++) {
        state->chatLines[i] = chatLines[currentOutputLine];
        state->chatLineColors[i] = chatLineColors[currentOutputLine];
        currentOutputLine = (currentOutputLine + 1) % MAX_CHAT_LINES;
    }
    return state;
}

static void swapLobbyStatePlayerSpot(LobbyState *state, int i1, int i2)
{
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

/* Always make sure our player appears to be first to GUI.
   Also make server appear to be second. Rest of the player are sorted by
   their normal positions. Doesn't apply if server (server should always be at
   0-th spot anyway). */
static LobbyState *putOurPlayerOnTop(LobbyState *state)
{
    int i;
    if (!weAreTheServer) {
        swapLobbyStatePlayerSpot(state, 0, ourPlayerIndex);
        for (i = ourPlayerIndex - 1; i >= 1; i--)
            swapLobbyStatePlayerSpot(state, i, i + 1);
    }
    return state;
}


/***

   Join game menu GUI interfacing.
   ===============================

***/

/* Called by GUI upon entering join game menu. We are given a function which
   will be used to update GUI after any changes. Start off with refreshing
   game list automatically, as if user pressed refresh button.
*/
void EnterWaitingToJoinState(SendWaitingToJoinReport updateFunc)
{
    assert(state == ST_NONE || state == ST_WAITING_TO_JOIN || state == ST_TRYING_TO_JOIN_GAME);
    state = ST_WAITING_TO_JOIN;
    joiningGame = -1;
    if (updateFunc)
        waitingToJoinReportFunc = updateFunc;
    /* initialize state */
    joinGamesState.refreshing = true;
    startSearchTick = lastRequestTick = currentTick;
    numWaitingGames = 0; /* clear it in case some games don't exist anymore */
    /* announce us to the world */
    SendListGamesRequest();
    SendWaitingGamesReport();
}

/* Called by GUI when the user is requesting games list refresh. */
void RefreshList()
{
    if (!joinGamesState.refreshing)
        EnterWaitingToJoinState(nullptr);
}

/* User is leaving joing games menu, cease all network activity. */
void LeaveWaitingToJoinState()
{
    state = ST_NONE;
    joinGamesState.refreshing = false;
    numWaitingGames = 0;
    CancelSendingPackets();
}

void JoinGame(int index, bool (*modalUpdateFunc)(int, const char *),
    void (*enterGameLobbyFunc)(), void (*disconnectedFunc)(), bool (*modalSyncFunc)(),
    void (*showTeamMenuFunc)(const char *, const char *), void (*onErrorFunc)(),
    void (*onGameEndFunc)())
{
    char *packet;
    int length;
    joiningGame = index;
    modalFunction = modalUpdateFunc;
    enterGameLobbyFunction = enterGameLobbyFunc;
    disconnectedFunction = disconnectedFunc;
    modalSyncFunction = modalSyncFunc;
    showTeamsMenuFunction = showTeamMenuFunc;
    onErrorFunction = onErrorFunc;
    onGameEndFunction = onGameEndFunc;
    ConnectTo(&waitingGames[index].address);    /* assume connected until proven otherwise */
    copyAddress(&connectedPlayers[0].address, &waitingGames[index].address);
    if (!LoadTeam(currentTeamId, connectedPlayers[0].team)) {    /* try to load the player's team */
        connectedPlayers[0].team[5] = '\0';
        currentTeamId = -1;
    }
    calla(Rand);
    packet = createJoinGamePacket(&length, joinId = D0, waitingGames[index].id,
        playerNick, connectedPlayers[0].team + 5);
    lastRequestTick = currentTick;      /* record to track timeout in connect */
    lastPingTime = 0;
    connectionError = false;
    SendSimplePacket(&waitingGames[index].address, packet, length);
    state = ST_TRYING_TO_JOIN_GAME;
}


/***

   Join game menu internal logic.
   ==============================

***/

/* Send broadcast request to which all open games on the network should respond. */
static void SendListGamesRequest()
{
    int length;
    char *request = createGetGameInfoPacket(&length);
    WriteToLog(("Sending broadcast request to list games, last tick is %d.", lastRequestTick));
    SendBroadcastPacket(request, length);
}

/* Send state report for GUI to update. */
static void SendWaitingGamesReport()
{
    int i;
    for (i = 0; i < numWaitingGames; i++)
        joinGamesState.waitingGames[i] = waitingGames[i].name;
    joinGamesState.waitingGames[i] = (char *)-1;    /* terminate with -1    */
    waitingToJoinReportFunc(&joinGamesState);       /* dispatch to the GUI  */
}

/* Send a broadcast request to find open games while we're in refresh mode, and
   at most once per defined interval (should be 1 second). */
static void UpdateRefreshing()
{
    if (joinGamesState.refreshing) {
        if (currentTick >= lastRequestTick + REFRESH_INTERVAL) {
            do {
                lastRequestTick += REFRESH_INTERVAL;
            } while (currentTick >= lastRequestTick + REFRESH_INTERVAL);
            SendListGamesRequest();
        }
        if (currentTick >= startSearchTick + GetNetworkTimeout()) {
            lastRequestTick = 0;
            joinGamesState.refreshing = false;
        }
    }
}

static void CheckForWaitingGameReply()
{
    word type;
    char *packet, respondingGameName[GAME_NAME_LENGTH + 10], *p;
    int length;
    IPX_Address node;
    int i;
    byte gameId;
    if ((packet = ReceivePacket(&length, &node))) {
        type = getRequestType(packet, length);
        /* we're only interested in game info data here */
        if (type == PT_GAME_INFO_DATA) {
            int networkVersion, networkSubversion, numPlayers, maxPlayers;
            /* danger Will Robinson, stack reserves are burning out */
            WriteToLog(("Stack available: %d", stackavail()));
            if (unpackGameInfoPacket(packet, length, &networkVersion, &networkSubversion,
                &numPlayers, &maxPlayers, respondingGameName, GAME_NAME_LENGTH + 1, &gameId)) {
                WriteToLog(("Game response received: %s, id = %d", respondingGameName, gameId));
                if (networkVersion > NETWORK_VERSION) {
                    WriteToLog(("Ignoring game with too new server (%d.%d)", networkVersion, networkSubversion));
                    qFree(packet);
                    return;
                }
                /* check if we already have this game */
                for (i = 0; i < numWaitingGames; i++)
                    if (!memcmp(&waitingGames[i].address, &node, sizeof(IPX_Address)))
                        break;
                if (i < MAX_GAMES_WAITING) {
                    /* add a new game/update old */
                    waitingGames[i].id = gameId;
                    p = strcpy(strcpy(strcpy(strcpy(waitingGames[i].name, respondingGameName),
                        " ("), int2str(numPlayers)), "/");
                    strcpy(strcpy(p, int2str(maxPlayers)), ")");
                    if (i >= numWaitingGames) {
                        memcpy(&waitingGames[i].address, &node, sizeof(node));
                        numWaitingGames++;
                    }
                }
            } else
                WriteToLog(("Rejecting malformed game info packet"));
        }
        qFree(packet);
    }
}

static bool JoiningGameOnIdle(LobbyState *lobbyState)
{
    char *packet, ourTeamName[MAX_TEAM_NAME_LEN];
    int i, packetSize;
    IPX_Address sender, *addresses[MAX_PLAYERS];
    byte errorCode;
    bool result;

    IPX_OnIdle();
    assert(modalFunction);
    if (connectionError) {
        if (modalFunction(-1, nullptr)) {
            EnterWaitingToJoinState(nullptr);
            return true;
        } else
            return false;
    }
    if ((packet = ReceivePacket(&packetSize, &sender)) && addressMatch(&sender, &waitingGames[joiningGame].address)) {
        /* server response to our join game attempt has arrived */
        switch (getRequestType(packet, packetSize)) {
        case PT_JOIN_GAME_REFUSED:
            WriteToLog(("Server not allowing us to join."));
            if (!unpackRefuseJoinGamePacket(packet, packetSize, &errorCode)) {
                WriteToLog(("Malformed game join refused packet received."));
                errorCode = 0;
            }
            errorCode = errorCode > sizeofarray(joinGameErrors) ? 0 : errorCode;
            WriteToLog(("Error: '%s'", joinGameErrors[errorCode]));
            connectionError = true;
            if (result = modalFunction(-1, joinGameErrors[errorCode]))
                EnterWaitingToJoinState(nullptr);
            qFree(packet);
            return result;
        case PT_JOIN_GAME_ACCEPTED:
            for (i = 0; i < MAX_PLAYERS; i++)
                addresses[i] = &connectedPlayers[i].address;
            *strncpy(ourTeamName, connectedPlayers[0].team + 5, MAX_TEAM_NAME_LEN) = '\0';
            if (unpackJoinedGameOkPacket(packet, packetSize, initLobbyState(lobbyState), addresses)) {
                NetPlayer *n = connectedPlayers + lobbyState->numPlayers;
                assert(lobbyState->numPlayers < MAX_PLAYERS);
                numConnectedPlayers = lobbyState->numPlayers;
                for (i = 0; i < numConnectedPlayers; i++)
                    connectedPlayers[i].flags = lobbyState->playerFlags[i];
                numConnectedPlayers = ++lobbyState->numPlayers;
                ourPlayerIndex = numConnectedPlayers - 1;
                n->flags = 0;
                memcpy(n->team, connectedPlayers[0].team, TEAM_SIZE);
                strcpy(n->name, playerNick);
                strcpy(n->team + 5, ourTeamName);
                /* save client options so they don't get overwritten by server options */
                if (!savedClientOptions)
                    savedClientOptions = qAlloc(sizeof(mpOptions));
                if (savedClientOptions)
                    memcpy(savedClientOptions, &mpOptions, sizeof(mpOptions));
                memcpy(&mpOptions, &lobbyState->options, sizeof(mpOptions));
                ApplyOptions(&mpOptions);
                n->userTactics = nullptr;
                modalFunction(1, nullptr);
                WriteToLog(("We joined!!!"));
                joinId++;   /* we can't use this id anymore */
                inLobby = true;
                qFree(packet);
                enterGameLobbyFunction();
                return true;
            } else {
                WriteToLog(("Rejecting malformed joined game packet."));
            }
        default:
            /* ignore */
            WriteToLog(("Ignoring packet (%#x) while waiting for join game result.", *(word *)packet));
            break;
        }
        qFree(packet);
    }
    assert(modalFunction);
    assert(enterGameLobbyFunction);
    /* parameter to modal func: -1 = done, error, 0 = not done, 1 = done, ok
       and it returns true if user wants to move on, false otherwise */
    if (currentTick > lastRequestTick + GetNetworkTimeout()) {
        /* still no response from server, abort */
        if (modalFunction(-1, "No response from the server.")) {
            EnterWaitingToJoinState(nullptr);
            return true;
        }
        connectionError = true;
    } else {
        /* resend, but be careful not to flood */
        static word lastResendTick;
        if (currentTick - lastResendTick > 25) {
            packet = createJoinGamePacket(&packetSize, joinId, waitingGames[joiningGame].id,
                playerNick, connectedPlayers[ourPlayerIndex].team + 5);
            SendSimplePacket(&waitingGames[joiningGame].address, packet, packetSize);
            lastResendTick = currentTick;
        }
        modalFunction(0, nullptr);
    }
    return false;
}


/***

   Sync state and setting up teams.
   ================================

***/

static void EnterSyncingState(bool (*modalSyncFunc)(int), void (*showTeamMenuFunc)(const char *, const char *))
{
    WriteToLog(("Going into syncing state..."));
    state = ST_SYNC;
    modalSyncFunction = modalSyncFunc;
    showTeamsMenuFunction = showTeamMenuFunc;
    lastRequestTick = currentTick;      /* reuse this */
    syncDone = false;
    if (isPlayer(ourPlayerIndex)) {
        if (!connectedPlayers[ourPlayerIndex].userTactics)
            connectedPlayers[ourPlayerIndex].userTactics = qAlloc(TACTICS_SIZE * 6);
        assert(connectedPlayers[ourPlayerIndex].userTactics);
        memcpy(connectedPlayers[ourPlayerIndex].userTactics, USER_A, TACTICS_SIZE * 6);
    }
    if (weAreTheServer && isPlayer(0)) {
        int i, length;
        char *packet = createTeamAndTacticsPacket(&length, connectedPlayers[0].team, USER_A);
        for (i = 1; i < numConnectedPlayers; i++) {
            //mozda neki resend fleg ako ne uspe?
            SendImportantPacket(&connectedPlayers[i].address, packet, length);
        }
    }
}

static void HandleNetworkKeys()
{
    char *packet;
    int length;
    IPX_Address node;
    byte controls;
    word longFireFlag;
    if (packet = ReceivePacket(&length, &node)) {
        switch (getRequestType(packet, length)) {
        case PT_CONTROLS:
            if (unpackControlsPacket(packet, length, &controls, &longFireFlag))
                AddControlsToNetworkQueue(controls, longFireFlag);
            else
                WriteToLog(("Invalid scan code packet ignored."));
            break;
        }
        qFree(packet);
    }
}

/* =============== */

/* Network loop during menus. Return false during blocking operations. */
bool NetworkOnIdle()
{
    UnAckPacket *timedOutPacket;
    int i;

    switch (state) {
    case ST_WAITING_TO_JOIN:
        UpdateRefreshing();
        CheckForWaitingGameReply();
        SendWaitingGamesReport();
        break;
    case ST_WAITING_TO_START:
        CheckIncomingServerLobbyTraffic();
        SendPingPackets();
        /* form state and send it to GUI to update */
        assert(updateLobbyFunction && weAreTheServer);
        updateLobbyFunction(putOurPlayerOnTop(initLobbyState(lobbyState)));
        break;
    case ST_TRYING_TO_JOIN_GAME:
        return JoiningGameOnIdle(lobbyState);
    case ST_GAME_LOBBY:
        if (!CheckIncomingClientLobbyTraffic())
            return false;
        PingServer();
        assert(updateLobbyFunction && !weAreTheServer);
        updateLobbyFunction(putOurPlayerOnTop(initLobbyState(lobbyState)));
        break;
    case ST_SYNC:
        return SyncOnIdle();
    case ST_PL1_SETUP_TEAM:
    case ST_PL2_SETUP_TEAM:
        HandleNetworkKeys();
        break;
    case ST_NONE:
        /* do nothing */
        return true;
    default:
        WriteToLog(("Unknown state: %s", stateToString(state)));
    }

    if (timedOutPacket = ResendUnacknowledgedPackets()) {
        WriteToLog(("Fatal network error, timed-out important packet [%#.12s], ticks = %hd, current ticks = %hd.",
            &timedOutPacket->address, timedOutPacket->time, currentTick));
        /* network error */
        switch (state) {
        case ST_WAITING_TO_START:
            /* client should never be in this state */
            assert(weAreTheServer);
            for (i = 1; i < numConnectedPlayers; i++)
                if (addressMatch(&connectedPlayers[i].address, &timedOutPacket->address))
                    break;
            if (i >= numConnectedPlayers)
                WriteToLog(("Double disconnect"));
            else {
                char buf[32];
                WriteToLog(("Disconnecting player: '%s' (timeout)", connectedPlayers[i].name));
                strcpy(strupr(strcpy(buf, connectedPlayers[i].name)), " LEFT.");
                HandleChatLine(-1, buf, 10);
                DisconnectClient(i, true);
            }
            break;
        case ST_GAME_LOBBY:
            /* client-only state */
            assert(!weAreTheServer);
            DisconnectClient(0, true);
            assert(disconnectedFunction);
            state = ST_NONE;
            disconnectedFunction();
            IPX_OnIdle();
            return false;
        case ST_SYNC:
        case ST_PL1_SETUP_TEAM:
        case ST_PL2_SETUP_TEAM:
            ;//nazad u lobi obavesti servera
            // ako je server diskonekt
            CleanUpPlayers();//? :P
            break;
        default:
            WriteToLog(("This state %#02x doesn't know how to handle timed out packets.", state));
            break;
        }
    }
    if (state != ST_NONE)
        IPX_OnIdle();
    return true;
}