/*
    Multiplayer module - hellz yeah!
*/

#include "qalloc.h"
#include "options.h"
#include "mplayer.h"
#include "mplobby.h"
#include "mpdirect.h"

static char m_playerNick[NICKNAME_LEN + 1];     /* player name for online games  */
static char m_gameName[GAME_NAME_LENGTH + 1];   /* name of current game          */

static const MP_Options m_defaultOptions = { DEFAULT_MP_OPTIONS };
static MP_Options m_mpOptions;
static MP_Options *m_savedClientOptions;

SavedTeamData m_savedTeamData[5];               /* we will keep saved positions for a few teams */

static byte m_numSubstitutes;
static byte m_maxSubstitutes;

static byte m_mpActive;
static dword m_currentTeamId = -1;

const char *InitMultiplayer()
{
    if (auto failureReason = InitializeNetwork())
        return failureReason;

    if (m_mpActive)
        return nullptr;

    calla_ebp_safe(SaveOptions);    /* save options here and not in menu to cover for alt-f1 exit */
    autoReplays = 0;                /* force auto-replays off */
    allPlayerTeamsEqual = 0;        /* force equal teams off  */
    g_gameType = 0;

    if (!m_mpOptions.size)
        m_mpOptions = m_defaultOptions;

    MP_Options options;
    ApplyMPOptions(GetMPOptions(&options));

    InitMultiplayerLobby();
    seed = g_currentTick;

    InitPlayerNick();
    InitGameName();

    /* assumes options have been loaded from the XML */
    SetSkipFrames(m_mpOptions.skipFrames);
    SetNetworkTimeout(m_mpOptions.networkTimeout);

    m_mpActive = true;

    return nullptr;
}

void FinishMultiplayer()
{
    if (m_mpActive) {
        calla_ebp_safe(RestoreOptions);
        FinishMultiplayerLobby();
        ShutDownNetwork();
        ReleaseClientMPOptions();
    }

    m_mpActive = false;
}

const char *GetGameName()
{
    auto directGameName = GetDirectGameName();
    if (directGameName[0])
        return directGameName;
    return m_gameName;
}

void SetGameName(const char *newGameName)
{
    *strncpy(m_gameName, newGameName, sizeof(m_gameName) - 1) = '\0';
}

const char *GetPlayerNick()
{
    auto directGameNickname = GetDirectGameNickname();
    if (directGameNickname[0])
        return directGameNickname;
    return m_playerNick;
}

void SetPlayerNick(const char *newPlayerNick)
{
    *strncpy(m_playerNick, newPlayerNick, sizeof(m_playerNick) - 1) = '\0';
}

void ApplySavedTeamData(TeamFile *team)
{
    for (size_t i = 0; i < sizeofarray(m_savedTeamData); i++) {
        dword teamId = *(dword *)team;
        if (teamId == m_savedTeamData[i].teamId) {
            WriteToLog("Found saved team data for %s", team->name);
            memcpy(team->playerOrder, m_savedTeamData[i].positions, sizeof(m_savedTeamData[i].positions));
            team->tactics = m_savedTeamData[i].tactics;
            return;
        }
    }

    WriteToLog("No saved team data for %s", team->name);
}

void StoreTeamData(const TeamFile *team)
{
    size_t i;
    const size_t kNumTeams = sizeofarray(m_savedTeamData);

    for (i = 0; i < kNumTeams; i++)
        if (team->getId() == m_savedTeamData[i].teamId)
            break;

    /* if all slots are taken, just overwrite any */
    if (i >= kNumTeams)
        i = g_currentTick % kNumTeams;

    assert(i < kNumTeams);
    WriteToLog("Storing data for team %s, %s", team->name, team->coachName);
    memcpy(m_savedTeamData[i].positions, team->playerOrder, sizeof(team->playerOrder));
    m_savedTeamData[i].tactics = team->tactics;
    m_savedTeamData[i].teamId = team->getId();
}

char *InitGameName()
{
    if (!m_gameName[0]) {
        unsigned short year;
        unsigned char month, day, hour, minute, second, hundred;
        GetDosDate(&year, &month, &day);
        GetDosTime(&hour, &minute, &second, &hundred);

        char *p = strcpy(strcpy(strcpy(m_gameName, "SWPP-"), int2str(year)), "-");
        p = strcpy(strcpy(p, int2str(month)), "-");
        p = strcpy(strcpy(p, int2str(day)), "-");
        p = strcpy(strcpy(p, int2str(hour)), ":");
        p = strcpy(strcpy(p, int2str(minute)), ":");
        p = strcpy(p, int2str(second));
        WriteToLog("Generated game name: %s", m_gameName);
    }

    return m_gameName;
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
    static_assert(sizeof(SavedTeamData) == 24 && sizeofarray(m_savedTeamData) == 5, "Saved positions changed.");
    memset(m_savedTeamData, -1, sizeof(m_savedTeamData));      /* default to -1 team ids */
    registerOptions("playMatch", 9, "Saved settings from play match menu", 35,
        "%n" "%24b/team1" "%24b/team2" "%24b/team3" "%24b/team4" "%24b/team5",
        &m_savedTeamData[0], &m_savedTeamData[1], &m_savedTeamData[2], &m_savedTeamData[3], &m_savedTeamData[4]);
}

/** RegisterNetworkOptions

    registerOptions -> callback to register multiplayer options to be saved/loaded.
*/
extern "C" void RegisterNetworkOptions(RegisterOptionsFunc registerOptions)
{
    registerOptions("multiplayer", 11, "Options for multiplayer games", 29,
        "%*s/playerNick%*s/gameName%4d/team", sizeof(m_playerNick), m_playerNick,
        sizeof(m_gameName), m_gameName, &m_currentTeamId);
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

    if (!m_playerNick[0]) {
        calla(Rand);
        int index = (dword)D0 * (sizeof(defaultNicks) / sizeof(defaultNicks[0]) - 1) / 255;
        WriteToLog("Random index = %d", index);
        char *p = strcpy(strcpy(m_playerNick, "SWOS "), defaultNicks[index]);
        calla(Rand);
        strcpy(strcpy(p, " "), int2str(D0));
        WriteToLog("Random name generated: %s", m_playerNick);
    }
}

/***

   Multiplayer options handling.
   =============================

***/

MP_Options *SetMPOptions(const MP_Options *newOptions)
{
    assert(newOptions->size == sizeof(m_mpOptions));
    memcpy(&m_mpOptions, newOptions, sizeof(m_mpOptions));

    return const_cast<MP_Options *>(newOptions);
}

/* Return currently active MP options for options manager. */
static MP_Options *GetMPOptionsPtr()
{
    if (!m_mpOptions.size)
        m_mpOptions = m_defaultOptions;

    assert(m_mpOptions.size == sizeof(m_mpOptions));
    return m_savedClientOptions ? m_savedClientOptions : &m_mpOptions;
}

/** GetMPOptions

    destOptions -> options struct where current multiplayer will be written

    This is safe version for external use where we hand out the copy instead of the pointer.
*/
MP_Options *GetMPOptions(MP_Options *destOptions)
{
    assert(m_mpOptions.size == sizeof(m_mpOptions));
    return (MP_Options *)memcpy(destOptions, &m_mpOptions, sizeof(m_mpOptions));
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
    m_numSubstitutes = options->numSubs;
    m_maxSubstitutes = options->maxSubs;
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
    options->numSubs = m_numSubstitutes;
    options->maxSubs = m_maxSubstitutes;
    options->skipFrames = GetSkipFrames();
    options->networkTimeout = GetNetworkTimeout();
    return options;
}

bool CompareMPOptions(const MP_Options *newOptions)
{
    assert(newOptions->size <= m_mpOptions.size && m_mpOptions.size == sizeof(m_mpOptions));
    return memcmp(m_savedClientOptions ? m_savedClientOptions : &m_mpOptions,
        newOptions, min(newOptions->size, sizeof(m_mpOptions))) == 0;
}

/** SaveClientOptions

    When the client joins server, it has to synchronize with server's game settings. Before that happens
    we then have to save original client's settings, otherwise single player settings would get overwritten too.
*/
void SaveClientMPOptions()
{
    if (!m_savedClientOptions)
        m_savedClientOptions = (MP_Options *)qAlloc(sizeof(m_mpOptions));

    if (m_savedClientOptions)
        memcpy(m_savedClientOptions, &m_mpOptions, sizeof(m_mpOptions));
}

void ReleaseClientMPOptions()
{
    qFree(m_savedClientOptions);
    m_savedClientOptions = nullptr;
}

int GetNumSubstitutes()
{
    return m_numSubstitutes;
}

int SetNumSubstitutes(byte newNumSubs)
{
    return m_numSubstitutes = newNumSubs;
}

int GetMaxSubstitutes()
{
    return m_maxSubstitutes;
}

int SetMaxSubstitutes(byte maxSubs)
{
    return m_maxSubstitutes = maxSubs;
}

void UpdateSkipFrames(int frames)
{
    m_mpOptions.skipFrames = frames;
}

void UpdateNetworkTimeout(word networkTimeout)
{
    m_mpOptions.networkTimeout = networkTimeout;
}

dword GetCurrentTeamId()
{
    return m_currentTeamId;
}

void SetCurrentTeamId(dword teamId)
{
    m_currentTeamId = teamId;
}
