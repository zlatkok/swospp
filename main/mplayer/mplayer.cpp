/*
    Multiplayer module - hellz yeah!
*/

#include "qalloc.h"
#include "options.h"
#include "mplayer.h"

static char playerNick[NICKNAME_LEN + 1];   /* player name for online games  */
static char gameName[GAME_NAME_LENGTH + 1]; /* name of current game          */

static const MP_Options defaultOptions = { DEFAULT_MP_OPTIONS };
static MP_Options mpOptions;
static MP_Options *savedClientOptions;

SavedTeamData savedTeamData[5];             /* we will keep saved positions for a few teams */

static byte numSubstitutes;
static byte maxSubstitutes;

static byte mpActive;

const char *InitMultiplayer()
{
    if (auto failureReason = InitializeNetwork())
        return failureReason;

    if (mpActive)
        return nullptr;

    calla_ebp_safe(SaveOptions);    /* save options here and not in menu to cover for alt-f1 exit */
    autoReplays = 0;                /* force auto-replays off */
    allPlayerTeamsEqual = 0;        /* force equal teams off  */
    gameType = 0;

    if (!mpOptions.size)
        mpOptions = defaultOptions;

    MP_Options options;
    ApplyMPOptions(GetMPOptions(&options));

    InitMultiplayerLobby();
    seed = currentTick;

    InitPlayerNick();
    InitGameName();

    /* assumes options have been loaded from the XML */
    SetSkipFrames(mpOptions.skipFrames);
    SetNetworkTimeout(mpOptions.networkTimeout);

    mpActive = true;

    return nullptr;
}


void FinishMultiplayer()
{
    if (mpActive) {
        calla_ebp_safe(RestoreOptions);
        FinishMultiplayerLobby();
        ShutDownNetwork();
        ReleaseClientMPOptions();
    }

    mpActive = false;
}


const char *GetGameName()
{
    auto directGameName = GetDirectGameName();
    if (directGameName[0])
        return directGameName;
    return gameName;
}


void SetGameName(const char *newGameName)
{
    *strncpy(gameName, newGameName, sizeof(gameName) - 1) = '\0';
}


const char *GetPlayerNick()
{
    auto directGameNickname = GetDirectGameNickname();
    if (directGameNickname[0])
        return directGameNickname;
    return playerNick;
}


void SetPlayerNick(const char *newPlayerNick)
{
    *strncpy(playerNick, newPlayerNick, sizeof(playerNick) - 1) = '\0';
}


void ApplySavedTeamData(TeamFile *team)
{
    for (size_t i = 0; i < sizeofarray(savedTeamData); i++) {
        dword teamId = *(dword *)team;
        if (teamId == savedTeamData[i].teamId) {
            WriteToLog("Found saved team data for %s", team->name);
            memcpy(team->playerOrder, savedTeamData[i].positions, sizeof(savedTeamData[i].positions));
            team->tactics = savedTeamData[i].tactics;
            return;
        }
    }

    WriteToLog("No saved team data for %s", team->name);
}


void StoreTeamData(const TeamFile *team)
{
    size_t i;
    const size_t kNumTeams = sizeofarray(savedTeamData);

    for (i = 0; i < kNumTeams; i++)
        if (team->getId() == savedTeamData[i].teamId)
            break;

    /* if all slots are taken, just overwrite any */
    if (i >= kNumTeams)
        i = currentTick % kNumTeams;

    assert(i < kNumTeams);
    WriteToLog("Storing data for team %s, %s", team->name, team->coachName);
    memcpy(savedTeamData[i].positions, team->playerOrder, sizeof(team->playerOrder));
    savedTeamData[i].tactics = team->tactics;
    savedTeamData[i].teamId = team->getId();
}


char *InitGameName()
{
    if (!gameName[0]) {
        unsigned short year;
        unsigned char month, day, hour, minute, second, hundred;
        GetDosDate(&year, &month, &day);
        GetDosTime(&hour, &minute, &second, &hundred);

        char *p = strcpy(strcpy(strcpy(gameName, "SWPP-"), int2str(year)), "-");
        p = strcpy(strcpy(p, int2str(month)), "-");
        p = strcpy(strcpy(p, int2str(day)), "-");
        p = strcpy(strcpy(p, int2str(hour)), ":");
        p = strcpy(strcpy(p, int2str(minute)), ":");
        p = strcpy(p, int2str(second));
        WriteToLog("Generated game name: %s", gameName);
    }

    return gameName;
}


static MP_Options *GetMPOptionsPtr();


/* Update this whenever MP_Options get changed! */
void registerMPOptions(RegisterOptionsFunc registerOptions)
{
    static_assert(sizeof(MP_Options) == 12, "Multi player options changed.");
    registerOptions("game", 4, "Default settings for multiplayer match", 38,
        "%n%c" "%2d/version" "%2d/gameLength" "%2d/pitchType" "%1d/numSubs" "%1d/maxSubs"
        "%1d/skipFrames" "%1d" "%2d/networkTimeout", GetMPOptionsPtr);
}


void registerPlayMatchMenuOptions(RegisterOptionsFunc registerOptions)
{
    static_assert(sizeof(SavedTeamData) == 24 && sizeofarray(savedTeamData) == 5, "Saved positions changed.");
    memset(savedTeamData, -1, sizeof(savedTeamData));      /* default to -1 team ids */
    registerOptions("playMatch", 9, "Saved settings from play match menu", 35,
        "%n" "%24b/team1" "%24b/team2" "%24b/team3" "%24b/team4" "%24b/team5",
        &savedTeamData[0], &savedTeamData[1], &savedTeamData[2], &savedTeamData[3], &savedTeamData[4]);
}


/** RegisterNetworkOptions

    registerOptions -> callback to register multiplayer options to be saved/loaded.
*/
extern "C" void RegisterNetworkOptions(RegisterOptionsFunc registerOptions)
{
    registerOptions("multiplayer", 11, "Options for multiplayer games", 29,
        "%*s/playerNick%*s/gameName%4d/team", sizeof(playerNick), playerNick,
        sizeof(gameName), gameName, &currentTeamId);
    registerMPOptions(registerOptions);
    registerPlayMatchMenuOptions(registerOptions);

}


/** Generate some funny random nicknames for starters, if nothing saved.
    Rand seed has to be initialized first. */
void InitPlayerNick()
{
    /* be aware of maximum name length when adding new nick here */
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
        calla(Rand);
        int index = (dword)D0 * (sizeof(defaultNicks) / sizeof(defaultNicks[0]) - 1) / 255;
        WriteToLog("Random index = %d", index);
        char *p = strcpy(strcpy(playerNick, "SWOS "), defaultNicks[index]);
        calla(Rand);
        strcpy(strcpy(p, " "), int2str(D0));
        WriteToLog("Random name generated: %s", playerNick);
    }
}


/***

   Multiplayer options handling.
   =============================

***/


MP_Options *SetMPOptions(const MP_Options *newOptions)
{
    assert(newOptions->size == sizeof(mpOptions));
    memcpy(&mpOptions, newOptions, sizeof(mpOptions));

    return const_cast<MP_Options *>(newOptions);
}


/* Return currently active MP options for options manager. */
static MP_Options *GetMPOptionsPtr()
{
    if (!mpOptions.size)
        mpOptions = defaultOptions;
    assert(mpOptions.size == sizeof(mpOptions));
    return savedClientOptions ? savedClientOptions : &mpOptions;
}


/** GetMPOptions

    destOptions -> options struct where current multiplayer will be written

    This is safe version for external use where we hand out the copy instead of the pointer.
*/
MP_Options *GetMPOptions(MP_Options *destOptions)
{
    assert(mpOptions.size == sizeof(mpOptions));
    return (MP_Options *)memcpy(destOptions, &mpOptions, sizeof(mpOptions));
}


/** ApplyMPOptions

    in:
        options -> initialized options to apply

    Apply given options to SWOS.
    When adding new MP options update these 2 functions. Besides updating MP_Options
    struct in both header and asm include file, default options will need to be updated,
    as well as MP_Options in LobbyState. Also update passing MP options to options manager.
*/
void ApplyMPOptions(const MP_Options *options)
{
    assert(options && options->size == sizeof(*options));
    gameLength = options->gameLength;
    pitchType = options->pitchType;
    numSubstitutes = options->numSubs;
    maxSubstitutes = options->maxSubs;
    SetSkipFrames(options->skipFrames);
    SetNetworkTimeout(options->networkTimeout);
}


/** GetFreshMPOptions

    in:
        options -> options struct that will get current values

    Read current values of options and store them into given options structure.
*/
MP_Options *GetFreshMPOptions(MP_Options *options)
{
    options->size = sizeof(*options);
    options->gameLength = gameLength;
    options->pitchType = pitchType;
    options->numSubs = numSubstitutes;
    options->maxSubs = maxSubstitutes;
    options->skipFrames = GetSkipFrames();
    options->networkTimeout = GetNetworkTimeout();
    return options;
}


bool CompareMPOptions(const MP_Options *newOptions)
{
    assert(newOptions->size <= mpOptions.size && mpOptions.size == sizeof(mpOptions));
    return memcmp(savedClientOptions ? savedClientOptions : &mpOptions,
        newOptions, min(newOptions->size, sizeof(mpOptions))) == 0;
}


/** SaveClientOptions

    When the client joins server, it has to synchronize with server's game settings. Before that happens
    we then have to save original client's settings, otherwise single player settings would get overwritten too.
*/
void SaveClientMPOptions()
{
    if (!savedClientOptions)
        savedClientOptions = (MP_Options *)qAlloc(sizeof(mpOptions));
    if (savedClientOptions)
        memcpy(savedClientOptions, &mpOptions, sizeof(mpOptions));
}


void ReleaseClientMPOptions()
{
    qFree(savedClientOptions);
    savedClientOptions = nullptr;
}


int GetNumSubstitutes()
{
    return numSubstitutes;
}


int SetNumSubstitutes(byte newNumSubs)
{
    return numSubstitutes = newNumSubs;
}


int GetMaxSubstitutes()
{
    return maxSubstitutes;
}


int SetMaxSubstitutes(byte newMaxSubs)
{
    return maxSubstitutes = newMaxSubs;
}


void UpdateSkipFrames(int frames)
{
    mpOptions.skipFrames = frames;
}


void UpdateNetworkTimeout(word networkTimeout)
{
    mpOptions.networkTimeout = networkTimeout;
}
