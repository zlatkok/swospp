#pragma once

#define NETWORK_VERSION     1   /* increase this for incompatible changes */
#define NETWORK_SUBVERSION  1

#define REFRESH_INTERVAL    70  /* ping new games after this many ticks */

#define MAX_PLAYERS     8       /* maximum number of players in multi-player game */

#define MAX_GAMES_WAITING       10
#define MAX_CHAT_LINES           9
#define MAX_CHAT_LINE_LENGTH    25
#define MAX_TEAM_NAME_LEN       17

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
    dword GetCurrentTeamId();
    void SetCurrentTeamId(dword teamId);

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

    int HandleMPKeys(int key);
}

#include "dosipx.h"

void GetRandomVariables(byte *vars, int *size);
void SetRandomVariables(const char *vars, int size);

/* Running multiplayer game */
extern "C" {
    void InitMultiplayerGame(int playerNo, IPX_Address *playerAddresses, int numWatchers,
        const IPX_Address *watcherAddresses, const Tactics *pl1CustomTactics, const Tactics *pl2CustomTactics);
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
