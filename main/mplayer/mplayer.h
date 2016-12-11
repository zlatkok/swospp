#pragma once

#define NETWORK_VERSION     1   /* increase this for incompatible changes */
#define NETWORK_SUBVERSION  1

#define REFRESH_INTERVAL    70  /* ping new games after this many ticks */

#define MAX_PLAYERS     8       /* maximum number of players in multi-player game */
#define NICKNAME_LEN    20      /* maximum multiplayer nickname length */
#define GAME_NAME_LENGTH NICKNAME_LEN + 10
#define MAX_GAMES_WAITING       10
#define MAX_CHAT_LINES           9
#define MAX_CHAT_LINE_LENGTH    25
#define MAX_TEAM_NAME_LEN       17

extern dword currentTeamId;                    /* id of current team */

/* structure used to communicate state of waiting games to GUI */
#pragma pack(push, 1)
struct WaitingToJoinReport {
    dword refreshing;
    char *waitingGames[MAX_GAMES_WAITING + 1];
};

struct MP_Options {
    word size;
    word gameLength;
    word pitchType;
    byte numSubs;
    byte maxSubs;
    byte skipFrames;
    byte padding[1];
    word networkTimeout;
};

/* saved data for a team in play match menu (player positions & selected tactics) */
struct SavedTeamData {
    dword teamId;
    dword tactics;
    byte positions[16];
};

#define DEFAULT_MP_OPTIONS sizeof(MP_Options), 1, 4, 2, 5, 2, 'z', 8,

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
typedef bool32 (*ModalFunction)(int status, const char *errorText);
typedef void (*EnterGameLobbyFunction)();
typedef void (*UpdateLobbyFunction)(const LobbyState *state);
typedef void (*ShowTeamsMenuFunction)(TeamFile *, TeamFile *, int (*)());

void ApplySavedTeamData(TeamFile *team);
void StoreTeamData(const TeamFile *team);

extern "C" {
    const char *InitMultiplayer();
    void FinishMultiplayer();
    void InitPlayerNick();
    char *InitGameName();
    const char *GetGameName();
    void SetGameName(const char *newGameName);
    const char *GetPlayerNick();
    void SetPlayerNick(const char *newPlayerNick);
    const char *GetDirectGameName();
    const char *GetDirectGameNickname();
    void InitializeMPOptions(const MP_Options *options);
    void UpdateMPOptions(const MP_Options *options);
    bool CompareMPOptions(const MP_Options *options);
    MP_Options *GetMPOptions(MP_Options *destOptions);
    MP_Options *SetMPOptions(const MP_Options *options);
    void ApplyMPOptions(const MP_Options *options);
    MP_Options *GetFreshMPOptions(MP_Options *options);
    void SaveClientMPOptions();
    void ReleaseClientMPOptions();
    int GetNumSubstitutes();
    int SetNumSubstitutes(byte newNumSubs);
    int GetMaxSubstitutes();
    int SetMaxSubstitutes(byte newMaxSubs);
    void UpdateSkipFrames(int frames);
    void UpdateNetworkTimeout(word networkTimeout);

    /* Game Lobby menu */
    void InitMultiplayerLobby();
    void FinishMultiplayerLobby();
    MP_Options *GetSavedClientOptions();
    void CreateNewGame(const MP_Options *options, void (*updateLobbyFunc)(const LobbyState *),
        void (*errorFunc)(), void (*onGameEndFunc)(), bool inWeAreTheServer);
    void DisbandGame();
    void GoBackToLobby();
    void SetPlayerReadyState(bool isReady);
    void SetPlayerOrWatcher(bool isWatcher);
    void SetupTeams(bool32 (*modalSyncFunc)(), ShowTeamsMenuFunction showTeamMenuFunc);
    bool32 CanGameStart();
    char *SetTeam(TeamFile *newTeamName, dword teamId);
    void AddChatLine(const char *line);
    void StartGame();
    void GameFinished();

    /* Multiplayer options menu */
    void InitializeMPOptionsMenu();
    void MPOptionsMenuAfterDraw();
    void NetworkTimeoutBeforeDraw();
    void SkipFramesBeforeDraw();
    void IncreaseNetworkTimeout();
    void DecreaseNetworkTimeout();
    void IncreaseSkipFrames();
    void DecreaseSkipFrames();
    void mpOptSelectTeamBeforeDraw();
    void ChooseMPTactics();
    void ExitMultiplayerOptions();

    /* Join game menu GUI interfacing */
    void EnterWaitingToJoinState(SendWaitingToJoinReport updateFunc = nullptr, int findGamesTimeout = 0);
    void RefreshList();
    void LeaveWaitingToJoinState();
    void JoinGame(int index, bool (*modalUpdateFunc)(int, const char *),
        void (*enterGameLobbyFunc)(), void (*disconnectedFunc)(), bool32 (*modalSyncFunc)(),
        ShowTeamsMenuFunction showTeamsMenuFunc, void (*onErrorFunc)(), void (*onGameEndFunc)());

    /* Network loop during menus. Return false during blocking operations. */
    bool32 NetworkOnIdle();
    void BroadcastControls(byte controls, word longFireFlag);
    dword GetControlsFromNetwork();
    int HandleMPKeys(int key);
    bool32 SwitchToNextControllingState();
}


#include "dosipx.h"

void GetRandomVariables(byte *vars, int *size);
void SetRandomVariables(const char *vars, int size);

/* Running multiplayer game */
extern "C" {
    void InitMultiplayerGame(int inPlayerNo, IPX_Address *inPlayerAddresses, int inNumWatchers,
        IPX_Address *inWatcherAddresses, Tactics *pl1CustomTactics, Tactics *pl2CustomTactics);
    void FinishMultiplayerGame();
    void OnGameLoopStart();
    void OnGameLoopEnd();
    byte GetGameStatus();
    int GetSkipFrames();
    int SetSkipFrames(int newSkipFrames);
}

/* User tactics bookkeeping */
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
