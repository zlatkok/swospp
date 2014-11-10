#pragma once

#define NETWORK_VERSION     1   /* increase this for incompatible changes      */
#define NETWORK_SUBVERSION  1

#define REFRESH_INTERVAL    70  /* ping new games after this many ticks        */

#define MAX_PLAYERS     8   /* maximum number of players in multi-player game  */
#define NICKNAME_LEN    20  /* maximum multiplayer nickname length             */
#define GAME_NAME_LENGTH NICKNAME_LEN + 10
#define MAX_GAMES_WAITING       10
#define MAX_CHAT_LINES           9
#define MAX_CHAT_LINE_LENGTH    25
#define MAX_TEAM_NAME_LEN       17

#pragma aux scanCodeQueue "*";
#pragma aux scanCodeQueueLength "*";

extern char playerNick[NICKNAME_LEN + 1];      /* player name for online games */
extern char gameName[GAME_NAME_LENGTH];        /* name of current game         */
extern dword currentTeamId;                    /* id of current team           */

/* structure used to communicate state of waiting games to GUI */
#pragma pack(push, 1)
typedef struct WaitingToJoinReport {
    dword refreshing;
    char *waitingGames[MAX_GAMES_WAITING + 1];
} WaitingToJoinReport;

typedef struct MP_Options {
    word size;
    word gameLength;
    word pitchType;
    byte numSubs;
    byte maxSubs;
    byte skipFrames;
    byte networkTimeout;
    byte padding[2];
} MP_Options;

#define DEFAULT_MP_OPTIONS sizeof(MP_Options), 1, 4, 2, 5, 2, 8, 'z', 'z'

typedef struct LobbyState {
    int numPlayers;
    MP_Options options;
    char *playerNames[MAX_PLAYERS];
    int playerFlags[MAX_PLAYERS];   /* bit 0 - is watcher, bit 1 - is ready  */
    char *playerTeamsNames[MAX_PLAYERS];
    char *chatLines[MAX_CHAT_LINES];
    byte chatLineColors[MAX_CHAT_LINES];
} LobbyState;
#pragma pack(pop)

enum MP_Join_Game_Response {
    GR_OK,
    GR_ROOM_FULL,
    GR_BANNED,
    GR_WRONG_VERSION,
    GR_CLOSED,
    GR_INSUFFICIENT_RESOURCES,
    GR_IN_PROGRRESS,
    GR_INVALID_GAME
};

/* GUI callbacks */
typedef void (*SendWaitingToJoinReport)(const WaitingToJoinReport *report);
typedef bool (*ModalFunction)(int status, const char *errorText);
typedef void (*EnterGameLobbyFunction)();
typedef void (*UpdateLobbyFunction)(const LobbyState *state);

void InitMultiplayer();
void FinishMultiplayer();
void InitPlayerNick();
char *GetPlayerNick();
char *InitGameName();
void DisbandGame();
bool CanGameStart();
char *SetTeam(char *newTeamName, dword teamIndex);
void AddChatLine(const char *line);
void StartGame();
void GameFinished();

/* Game Lobby menu */
void CreateNewGame(const MP_Options *options, void (*updateLobbyFunc)(const LobbyState *),
    void (*errorFunc)(), void (*onGameEndFunc)(), bool inWeAreTheServer);
void InitializeMPOptions(const MP_Options *options);
void UpdateMPOptions(const MP_Options *options);
bool CompareMPOptions(const MP_Options *options);
MP_Options *GetMPOptions(MP_Options *options);

/* Join game menu GUI interfacing */
void EnterWaitingToJoinState(SendWaitingToJoinReport updateFunc);
void RefreshList();
void LeaveWaitingToJoinState();
void JoinGame(int index, bool (*modalUpdateFunc)(int, const char *),
    void (*enterGameLobbyFunc)(), void (*disconnectedFunc)(),
    bool (*modalSyncFunc)(), void (*startGameFunc)(),
    void (*onErrorFunc)(bool), void (*onGameEndFunc)());

/* Network loop during menus. Return false during blocking operations. */
bool NetworkOnIdle();

#include "dosipx.h"

void GetRandomVariables(char *vars, int *size);
void SetRandomVariables(const char *vars, int size);

/* Running multiplayer game */
void InitMultiplayerGame(int inPlayerNo, IPX_Address *inPlayerAddresses, int inNumWatchers,
    IPX_Address *inWatcherAddresses, Tactics *pl1CustomTactics, Tactics *pl2CustomTactics);
void FinishMultiplayerGame();
void OnGameLoopStart();
void OnGameLoopEnd();
byte GetGameStatus();
int GetSkipFrames();
void SetSkipFrames(int newSkipFrames);

/* Options bookkeeping */
bool ValidateUserMpTactics();
Tactics *GetUserMpTactics();

/* return code from the game */
enum gameStatus {
    GS_GAME_IN_PROGRESS,
    GS_GAME_FINISHED_OK,
    GS_GAME_DISCONNECTED,
    GS_PLAYER1_ABORTED,
    GS_PLAYER2_ABORTED,
    GS_WATCHER_ABORTED,
    GS_WATCHER_ENDED
};