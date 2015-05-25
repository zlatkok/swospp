/** mpoptmnu.c

    Multiplayer options menu event handlers.
*/

#include "swos.h"
#include "util.h"
#include "dosipx.h"
#include "mplayer.h"
#include "options.h"

/* has to be public for the menu */
Tactics MP_Tactics[6];

/** initMpTactic

    tactic -> tactic to initialize
    name   -> name of the tactic

    Initialize single user tactic to "factory settings". That means copy it
    from 4-4-2, and set name to USER_[A-F].
*/
static void initMpTactic(Tactics *tactic, const char *name)
{
    memcpy(tactic, tact_4_4_2, sizeof(Tactics));
    strcpy((char *)tactic, name);
}

/** initMpTactics

    Put user tactics for multiplayer in initial state. We duplicate SWOS initialization
    but we must must since it's not run until menu initialization.
*/
static void initMpTactics()
{
    int i;
    for (i = 0; i < TACTICS_USER_F - TACTICS_USER_A + 1; i++)
        initMpTactic(MP_Tactics + i, tacticsStringTable[TACTICS_USER_A + i]);
    calla(InitializeTacticsPositions);
}

extern "C" void RegisterUserTactics(RegisterOptionsFunc registerOptions)
{
    initMpTactics();
    registerOptions("tactics", 7, "User tactics in multiplayer games", 33, "%n"
        "%370b/USER_A" "%370b/USER_B" "%370b/USER_C"
        "%370b/USER_D" "%370b/USER_E" "%370b/USER_F",
        &MP_Tactics[0], &MP_Tactics[1], &MP_Tactics[2], &MP_Tactics[3], &MP_Tactics[4], &MP_Tactics[5]);
}

Tactics *GetUserMpTactics()
{
    return MP_Tactics;
}

/** validateUserMpTactic

    Validate single user tactic, return true if it passes.
*/
static bool validateUserMpTactic(const Tactics *tactics)
{
    char c = 0;
    /* check if name is all zeros */
    for (size_t i = 0; i < sizeof(tactics->name); i++)
        c |= tactics->name[i];
    if (!c) {
        WriteToLog(("User tactics without name found."));
        return false;
    }
    if (tactics->ballOutOfPlayTactics >= TACTICS_MAX) {
        WriteToLog(("Invalid out of play tactics found."));
        return false;
    }
    return true;
}

/** ValidateUserMpTactics

    Validate all user tactics. Call it right after saved user tactics are loaded.
    Return true if tactics are ok, false if there had to be some changes.
*/
bool ValidateUserMpTactics()
{
    bool modified = true;
    for (size_t i = 0; i < sizeofarray(MP_Tactics); i++) {
        if (!validateUserMpTactic(MP_Tactics + i)) {
            initMpTactic(MP_Tactics + i, tacticsStringTable[TACTICS_USER_A + i]);
            modified = false;
        }
    }
    return modified;
}

static char *LoadTeam()
{
    int teamNo = currentTeamId & 0xff, ordinal = currentTeamId >> 8 & 0xff;
    D0 = teamNo;
    calla(LoadTeamFile);
    assert(D0);
    return teamFileBuffer + 2 + ordinal * TEAM_SIZE;
}

void InitializeMPOptionsMenu()
{
    /* stoopid DrawSprite16Pixels has hardcoded width of 384 so patch it temporarily so we can draw player icons */
    static const byte patchDrawSprite[9] = { 0xb8, 0x40, 0x01, 0x00, 0x00, 0xf7, 0xe7, 0x8b, 0xf8 };
    memcpy((char *)SWOS_DrawSprite16Pixels + 0x116, patchDrawSprite, sizeof(patchDrawSprite));
}

void NetworkTimeoutBeforeDraw()
{
    MenuEntry *entry = (MenuEntry *)A5;
    strcpy(strcpy(entry->u2.string, int2str(GetNetworkTimeout() / 70)), " SECONDS");
}

void SkipFramesBeforeDraw()
{
    MenuEntry *entry = (MenuEntry *)A5;
    int numFrames = GetSkipFrames();
    strcpy(strcpy(strcpy(entry->u2.string, int2str(numFrames)), " FRAME"), numFrames == 1 ? "" : "S");
}

void IncreaseNetworkTimeout()
{
    SetNetworkTimeout(GetNetworkTimeout() + 70);
}

void DecreaseNetworkTimeout()
{
    SetNetworkTimeout(GetNetworkTimeout() - 70);
}

void IncreaseSkipFrames()
{
    SetSkipFrames(GetSkipFrames() + 1);
}

void DecreaseSkipFrames()
{
    SetSkipFrames(GetSkipFrames() - 1);
}

extern char aTeamNotChosen[] asm("aTeamNotChosen");

void mpOptSelectTeamBeforeDraw()
{
    MenuEntry *entry = (MenuEntry *)A5;
    entry->u2.string = currentTeamId == (dword)-1 ? aTeamNotChosen : LoadTeam() + 5;
}

/** ChooseMPTactics

    Invoked when user selects a user tactic to edit in multiplayer options menu.
*/
void ChooseMPTactics()
{
    int tacticsIndex;
    MenuEntry *entry = (MenuEntry *)A5;
    if (currentTeamId == (dword)-1) {
        A0 = (dword)"PLEASE CHOOSE A TEAM FIRST";
        calla(ShowErrorMenu);
        return;
    }
    tacticsIndex = (entry->u2.string - (char *)MP_Tactics) / TACTIC_SIZE;
    assert(tacticsIndex >= 0 && tacticsIndex < 6);
    chosenTactics = TACTICS_USER_A + tacticsIndex;
    WriteToLog(("Editing tactics: %s", entry->u2.string));
    chooseTacticsTeamPtr = LoadTeam();
    calla(InitLittlePlayerSprites);
    A6 = (dword)editTacticsMenu;
    calla(ShowMenu);
    MP_Tactics[tacticsIndex] = *editTacticsCurrentTactics;
    WriteToLog(("Finished editing tactics: %s", editTacticsCurrentTactics));
}

void ExitMultiplayerOptions()
{
    /* unpatch sprite drawing */
    static const byte unpatchDrawSprite[9] = { 0xc1, 0xe7, 0x07, 0x8b, 0xc7, 0xd1, 0xe7, 0x03, 0xf8 };
    memcpy((char *)SWOS_DrawSprite16Pixels + 0x116, unpatchDrawSprite, sizeof(unpatchDrawSprite));
    SaveOptionsIfNeeded();
    calla(SetExitMenuFlag);
}