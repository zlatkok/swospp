#include "swos.h"
#include "util.h"
#include "mptact.h"

/** Virtual tactics support - it will appear to each player that they are using their own custom tactics,
    and will have no notion of the other player's tactics. That is accomplished by switching tactics
    every time we need to show formation menu - only custom tactics of the player that requested bench
    will be showing.
*/

static const Tactics *pl1MPCustomTactics;   /* point to actual custom tactics */
static const Tactics *pl2MPCustomTactics;
static byte pl1RealTactics;                 /* real player tactics, without mucking */
static byte pl2RealTactics;
static byte * const plRealTactics[2] = { &pl1RealTactics, &pl2RealTactics };

static void VirtualizeTactics(int teamNo, word newTactics, TeamGeneralInfo *team);
static void RestoreTactics();
static void OnChangeTactics(word newTactics, TeamGeneralInfo *benchTeam);
#pragma aux OnChangeTactics parm [eax esi];
static void __declspec(naked) SkipTacticsIinit();


void InitTacticsContextSwitcher(const Tactics *player1CustomTactics, const Tactics *player2CustomTactics)
{
    assert(playMatchTeam1Ptr && playMatchTeam2Ptr);
    /* save real tactics first */
    pl1RealTactics = playMatchTeam1Ptr->tactics;
    pl2RealTactics = playMatchTeam2Ptr->tactics;
    WriteToLog(("Initial tactics: %d (\"%s\"), %d (\"%s\")", pl1RealTactics, tacticsTable[pl1RealTactics],
        pl2RealTactics, tacticsTable[pl2RealTactics]));
    pl1MPCustomTactics = player1CustomTactics;
    pl2MPCustomTactics = player2CustomTactics;
    pl1Tactics = pl1RealTactics;
    pl2Tactics = pl2RealTactics;
    /* let's keep tactics 1 as fixed at start */
    VirtualizeTactics(2, pl2RealTactics, rightTeamData);
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
void SkipTacticsIinit()
{
    _asm {
        pop  esi                        ; skip tactics init (we already did it)
        mov  word ptr [esi - 5], 0x358b ; patch it back as it was
        mov  [esi - 3], offset A1
        retn
    }
}

/** VirtualizeTactics

    teamNo -  number of team whose tactics are about to be changed
    team   -> bench team

    Apply newTactics to teamNo. Bring tactics to "playable" state, meaning each team will
    effectively use whichever tactics their player chose. Keep effective tactics in plTactics,
    and actual tactics in plRealTactics. Do not modify other player's tactics.
*/
static void VirtualizeTactics(int teamNo, word newTactics, TeamGeneralInfo *team)
{
    static Tactics const **plMPCustomTactics[2] = { &pl1MPCustomTactics, &pl2MPCustomTactics };
    static word * const plTactics[2] = { &pl1Tactics, &pl2Tactics };
    int otherTeam = --teamNo ^ 1;
    const Tactics *thisTeamCustomTactics = *plMPCustomTactics[teamNo];
    const Tactics *otherTeamCustomTactics = *plMPCustomTactics[otherTeam];
    assert(team);
    assert(teamNo == 0 || teamNo == 1);
    assert(pl1Tactics < TACTICS_MAX && pl2Tactics < TACTICS_MAX && newTactics < TACTICS_MAX);
    assert(pl1RealTactics < TACTICS_MAX && pl2RealTactics < TACTICS_MAX);

    /* store original new tactics immediately, before we ruin it :P */
    *plRealTactics[teamNo] = newTactics;
    /* for other team only fix tactics name in case it's custom */
    if (*plTactics[otherTeam] >= TACTICS_USER_A)
        *tacticsTable[*plTactics[otherTeam]] = otherTeamCustomTactics[*plRealTactics[otherTeam] - TACTICS_USER_A];
    /* check if new tactics are custom, if so check if there's a conflict and resolve it */
    if (newTactics >= TACTICS_USER_A) {
        if ((newTactics += newTactics == *plTactics[otherTeam]) == TACTICS_MAX)
            newTactics = TACTICS_MAX - 2;
        *tacticsTable[newTactics] = thisTeamCustomTactics[*plRealTactics[teamNo] - TACTICS_USER_A];
    }
    /* store new, possibly adjusted tactics */
    *plTactics[teamNo] = newTactics;
    team->tactics = newTactics;
}

/* Return index of player which requested bench. */
static int getBenchTeamIndex()
{
    assert(benchTeam && (benchTeam->playerNumber == 1 || benchTeam->playerNumber == 2));
    return (benchTeam == rightTeamData) ^ (teamPlayingUp == 2);
}

/* Restore tactics to "being able to show" state. */
static void RestoreTactics()
{
    static const Tactics **customTactics[2] = { &pl1MPCustomTactics, &pl2MPCustomTactics };
    int benchTeamIndex = getBenchTeamIndex();
    int i;

    assert(pl1Tactics < TACTICS_MAX && pl2Tactics < TACTICS_MAX);
    assert(teamPlayingUp == 1 || teamPlayingUp == 2);
    assert(benchTeamIndex == 0 || benchTeamIndex == 1);
    assert(pl1MPCustomTactics && pl2MPCustomTactics);

    for (i = 0; i < 6; i++)
        *strncpy(tacticsTable[TACTICS_USER_A + i]->name,
            (*customTactics[benchTeamIndex])[i].name, member_size(Tactics, name) - 1) = '\0';

    /* show real index regardless of what we really have set it to */
    selectedFormationEntry = *plRealTactics[benchTeamIndex];
}

/** OnChangeTactics

    newTactics -  new tactics to apply, what user selected from menu
    benchTeam  -> team that requested bench (and to which tactics change applies)

    Player has selected new tactics from formation menu.
*/
static void OnChangeTactics(word newTactics, TeamGeneralInfo *benchTeam)
{
    int teamNo = getBenchTeamIndex();
    assert(benchTeam && (benchTeam->playerNumber == 1 || benchTeam->playerNumber == 2));
    VirtualizeTactics(teamNo + 1, newTactics, benchTeam);
}