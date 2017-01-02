#pragma once

#include "mplayer.h"
#include "netplayer.h"

enum MP_State {
    ST_NONE = 0x1123,
    ST_WAITING_TO_JOIN,
    ST_WAITING_TO_START,                /* state for server */
    ST_TRYING_TO_JOIN_GAME,
    ST_GAME_LOBBY,                      /* state for client */
    ST_SYNC,
    ST_PL1_SETUP_TEAM,
    ST_PL2_SETUP_TEAM,
    ST_CONFIRMING_KEYS,
    ST_GAME,
    ST_MAX,
};

#pragma pack(push, 1)
struct LobbyState {
    int numPlayers;
    MP_Options options;
    char *playerNames[MAX_PLAYERS];
    int playerFlags[MAX_PLAYERS];   /* bit 0 - is watcher, bit 1 - is ready  */
    char *playerTeamsNames[MAX_PLAYERS];
    char *chatLines[MAX_CHAT_LINES];
    byte chatLineColors[MAX_CHAT_LINES];
};
#pragma pack(pop)


/* Read/write internal state */
NetPlayer *GetPlayer(int index);
IPX_Address *GetControllingPlayerAddress();
int GetNumPlayers();
int GetOurPlayerIndex();
bool IsOurPlayerControlling();
void ControllingPlayerTimedOut(int i = -1);
int GetCurrentPlayerTeamIndex();
MP_State GetState();
void SwitchToState(MP_State state);

#ifdef DEBUG
const char *StateToString(enum MP_State state);
#endif

/* GUI callbacks */
typedef void (*SendWaitingToJoinReport)(const WaitingToJoinReport *report);
typedef bool32 (*ModalFunction)(int status, const char *errorText);
typedef void (*EnterGameLobbyFunction)();
typedef void (*UpdateLobbyFunction)(const LobbyState *state);
typedef void (*ShowTeamsMenuFunction)(TeamFile *, TeamFile *, int (*)());
typedef bool32 (*SetupTeamsModalFunction)();
typedef void (*SetupTeamsOnErrorFunction)(bool);


extern "C" {
    /* Game Lobby menu */
    void InitMultiplayerLobby();
    void FinishMultiplayerLobby();
    void CreateNewGame(const MP_Options *options, void (*updateLobbyFunc)(const LobbyState *),
        void (*errorFunc)(), void (*onGameEndFunc)(), bool weAreTheServer);
    void DisbandGame();
    void GoBackToLobby();
    void SetPlayerReadyState(bool isReady);
    void SetPlayerOrWatcher(bool isWatcher);
    void SetupTeams(void (*modalSyncFunc)(), ShowTeamsMenuFunction showTeamMenuFunc,
        SetupTeamsModalFunction setupTeamsModalFunc, SetupTeamsOnErrorFunction setupTeamsOnErrorFunc);
    bool32 CanGameStart();
    char *SetTeam(TeamFile *newTeamName, dword teamId);
    void AddChatLine(const char *line);
    void StartGame();
    void GameFinished();

    /* Join game menu GUI interfacing */
    void EnterWaitingToJoinState(SendWaitingToJoinReport updateFunc = nullptr, int findGamesTimeout = 0);
    void RefreshList();
    void LeaveWaitingToJoinState();
    void JoinGame(int index, bool (*modalUpdateFunc)(int, const char *),
        void (*enterGameLobbyFunc)(), void (*disconnectedFunc)(), void (*modalSyncFunc)(),
        ShowTeamsMenuFunction showTeamsMenuFunc, void (*onErrorFunc)(), void (*onGameEndFunc)(),
        SetupTeamsModalFunction setupTeamsModalFunc, SetupTeamsOnErrorFunction setupTeamsOnErrorFunc);

    /* Network loop during menus. Return false during blocking operations. */
    bool32 NetworkOnIdle();
}
