/** mptact.cpp

    Virtual tactics support - it will appear to each player that they are using their own custom tactics,
    and will have no notion of the other player's tactics. That is accomplished by switching tactics
    every time we need to show formation menu - only custom tactics of the player that requested bench
    will be showing.
*/

#include "mptact.h"

static const Tactics *m_pl1MPCustomTactics;     /* point to actual custom tactics */
static const Tactics *m_pl2MPCustomTactics;
static byte m_pl1RealTactics;                   /* real player tactics, without mucking */
static byte m_pl2RealTactics;
static byte * const m_plRealTactics[2] = { &m_pl1RealTactics, &m_pl2RealTactics };

static void VirtualizeTactics(int teamNo, word newTactics, TeamGeneralInfo *team);
static void RestoreTactics();
static void OnChangeTactics(word newTactics, TeamGeneralInfo *benchTeam);
void SkipTacticsIinit() asm("SkipTacticsIinit");

void InitTacticsContextSwitcher(const Tactics *player1CustomTactics, const Tactics *player2CustomTactics)
{
    assert(playMatchTeam1Ptr && playMatchTeam2Ptr);

    /* save real tactics first, they're swapped initially */
    m_pl1RealTactics = playMatchTeam2Ptr->tactics;
    m_pl2RealTactics = playMatchTeam1Ptr->tactics;
    WriteToLog("Initial tactics: %s - %d (\"%s\"), %s - %d (\"%s\")", playMatchTeam1Ptr->name, m_pl1RealTactics,
        g_tacticsTable[m_pl1RealTactics], playMatchTeam2Ptr->name, m_pl2RealTactics, g_tacticsTable[m_pl2RealTactics]);
    m_pl1MPCustomTactics = player1CustomTactics;
    m_pl2MPCustomTactics = player2CustomTactics;
    pl1Tactics = m_pl1RealTactics;
    pl2Tactics = m_pl2RealTactics;

    /* let's keep tactics 1 as fixed at start */
    VirtualizeTactics(2, m_pl2RealTactics, &rightTeamData);

    /* overwrite selectedFormationEntry assignment with our call */
    PatchCall(ShowFormationMenu, 0x27, RestoreTactics);
    PatchByte(ShowFormationMenu, 0x26, 0xe8);
    PatchByte(ShowFormationMenu, 0x2b, 0x90);

    /* hook new tactics selection, for both players */
    PatchCall(ChangeFormationIfFiring, 0x3f, OnChangeTactics);
    PatchByte(ChangeFormationIfFiring, 0x3e, 0xe8);
    PatchByte(ChangeFormationIfFiring, 0x43, 0x90);

    PatchCall(ChangeFormationIfFiring, 0x118, OnChangeTactics);
    PatchByte(ChangeFormationIfFiring, 0x117, 0xe8);
    PatchByte(ChangeFormationIfFiring, 0x11c, 0x90);

    /* prevent initialization of player tactics, since our init is called before; this one unpatches itself */
    PatchByte(SetupPlayers, 0x465, 0xe8);
    PatchCall(SetupPlayers, 0x466, SkipTacticsIinit);
}

void DisposeTacticsContextSwitcher()
{
    PatchDword(ShowFormationMenu, 0x26, 0x189ea366);
    PatchWord(ShowFormationMenu, 0x2a, 0x0017);
    PatchWord(ChangeFormationIfFiring, 0x3e, 0xa366);
    PatchDword(ChangeFormationIfFiring, 0x40, &pl1Tactics);
    PatchWord(ChangeFormationIfFiring, 0x117, 0xa366);
    PatchDword(ChangeFormationIfFiring, 0x119, &pl2Tactics);
}

/* Discard return address (return to caller's caller), and unpatch us. */
asm (
"SkipTacticsIinit:                      \n"
    "pop  esi                           \n"     // skip tactics init (we already did it)
    "mov  word ptr [esi - 5], 0x358b    \n"     // patch it back as it was
    "mov  dword ptr [esi - 3], offset A1 \n"
    "ret                                \n"
);

/** VirtualizeTactics

    teamNo -  number of team whose tactics are about to be changed
    team   -> bench team

    Apply newTactics to teamNo. Bring tactics to "playable" state, meaning each team will
    effectively use whichever tactics their player chose. Keep effective tactics in plTactics,
    and actual tactics in plRealTactics. Do not modify other player's tactics.
*/
static void VirtualizeTactics(int teamNo, word newTactics, TeamGeneralInfo *team)
{
    static Tactics const **plMPCustomTactics[2] = { &m_pl1MPCustomTactics, &m_pl2MPCustomTactics };
    static word * const plTactics[2] = { &pl1Tactics, &pl2Tactics };
    int otherTeam = --teamNo ^ 1;

    const Tactics *thisTeamCustomTactics = *plMPCustomTactics[teamNo];
    const Tactics *otherTeamCustomTactics = *plMPCustomTactics[otherTeam];

    assert(team);
    assert(teamNo == 0 || teamNo == 1);
    assert(pl1Tactics < TACTICS_MAX && pl2Tactics < TACTICS_MAX && newTactics < TACTICS_MAX);
    assert(m_pl1RealTactics < TACTICS_MAX && m_pl2RealTactics < TACTICS_MAX);

    /* store original new tactics immediately, before we ruin it :P */
    *m_plRealTactics[teamNo] = newTactics;

    /* for other team only fix tactics name in case it's custom */
    if (*plTactics[otherTeam] >= TACTICS_USER_A)
        *g_tacticsTable[*plTactics[otherTeam]] = otherTeamCustomTactics[*m_plRealTactics[otherTeam] - TACTICS_USER_A];

    /* check if new tactics are custom, if so check if there's a conflict and resolve it */
    if (newTactics >= TACTICS_USER_A) {
        if ((newTactics += newTactics == *plTactics[otherTeam]) == TACTICS_MAX)
            newTactics = TACTICS_MAX - 2;
        *g_tacticsTable[newTactics] = thisTeamCustomTactics[*m_plRealTactics[teamNo] - TACTICS_USER_A];
    }

    /* store new, possibly adjusted tactics */
    *plTactics[teamNo] = newTactics;
    team->tactics = newTactics;
}

/* Return index of player which requested bench. */
static int getBenchTeamIndex()
{
    assert(benchTeam && (benchTeam->playerNumber == 1 || benchTeam->playerNumber == 2));
    return (benchTeam == &rightTeamData) ^ (teamPlayingUp == 2);
}

/* Restore tactics to "being able to show" state. Called when player opens up bench menu. */
static void RestoreTactics()
{
    static const Tactics **customTactics[2] = { &m_pl1MPCustomTactics, &m_pl2MPCustomTactics };
    int benchTeamIndex = getBenchTeamIndex();

    assert(pl1Tactics < TACTICS_MAX && pl2Tactics < TACTICS_MAX);
    assert(teamPlayingUp == 1 || teamPlayingUp == 2);
    assert(benchTeamIndex == 0 || benchTeamIndex == 1);
    assert(m_pl1MPCustomTactics && m_pl2MPCustomTactics);

    for (int i = 0; i < 6; i++)
        *strncpy(g_tacticsTable[TACTICS_USER_A + i]->name,
            (*customTactics[benchTeamIndex])[i].name, sizeof(Tactics::name) - 1) = '\0';

    /* show real index regardless of what we have set it to */
    selectedFormationEntry = *m_plRealTactics[benchTeamIndex];
}

/** OnChangeTactics

    newTactics -  (in eax) new tactics to apply, what user selected from menu
    benchTeam  -> (in esi) team that requested bench (and to which tactics change applies)

    Player has selected new tactics from formation menu. Called from SWOS.
*/
static void OnChangeTactics(word newTactics, TeamGeneralInfo *benchTeam)
{
    /* force param to esi */
    asm volatile(
        ""
        : "=S" (benchTeam)
        : "r" (benchTeam)
        :
    );
    int teamNo = getBenchTeamIndex();
    assert(benchTeam && (benchTeam->playerNumber == 1 || benchTeam->playerNumber == 2));
    VirtualizeTactics(teamNo + 1, newTactics, benchTeam);
}
