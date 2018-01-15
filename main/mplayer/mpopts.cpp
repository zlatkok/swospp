/** mpopts.cpp

    Multiplayer options menu event handlers.
*/

#include "dosipx.h"
#include "mplayer.h"
#include "options.h"

/* has to be public for the menu */
Tactics MP_Tactics[6];

/** InitMpTactic

    tactic -> tactic to initialize
    name   -> name of the tactic

    Initialize single user tactic to "factory settings". That means copy it
    from 4-4-2, and set name to USER_[A-F].
*/
static void InitMpTactic(Tactics *tactic, const char *name)
{
    memcpy(tactic, tact_4_4_2, sizeof(Tactics));
    strcpy((char *)tactic, name);
}

/** InitMpTactics

    Put user tactics for multiplayer in initial state. We duplicate SWOS initialization
    but we must wait, since it's not run until menu initialization.
*/
static void InitMpTactics()
{
    for (int i = 0; i < TACTICS_USER_F - TACTICS_USER_A + 1; i++)
        InitMpTactic(MP_Tactics + i, tacticsStringTable[TACTICS_USER_A + i]);

    calla_ebp_safe(InitializeTacticsPositions);
}

extern "C" void RegisterUserTactics(RegisterOptionsFunc registerOptions)
{
    InitMpTactics();
    registerOptions("tactics", 7, "User tactics in multiplayer games", 33, "%n"
        "%370b/USER_A" "%370b/USER_B" "%370b/USER_C"
        "%370b/USER_D" "%370b/USER_E" "%370b/USER_F",
        &MP_Tactics[0], &MP_Tactics[1], &MP_Tactics[2], &MP_Tactics[3], &MP_Tactics[4], &MP_Tactics[5]);
}

Tactics *GetUserMpTactics()
{
    return MP_Tactics;
}

/** ValidateUserMpTactic

    Validate single user tactic, return true if it passes.
*/
static bool ValidateUserMpTactic(const Tactics *tactics)
{
    char c = 0;

    /* check if name is all zeros */
    for (size_t i = 0; i < sizeof(tactics->name); i++)
        c |= tactics->name[i];

    if (!c) {
        WriteToLog("User tactics without name found.");
        return false;
    }

    /* check for out of range player quadrant */
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 35; j++) {
            byte pos = tactics->playerPos[i].positions[j];
            auto xPos = pos >> 4;
            if (xPos > 14) {
                WriteToLog("Invalid player x quadrant, for player %d, ball position %d in tactics %s", i, j, tactics->name);
                return false;
            }
        }
    }

    if (tactics->ballOutOfPlayTactics >= TACTICS_MAX) {
        WriteToLog("Invalid out of play tactics found for %s.", tactics->name);
        return false;
    }

    return true;
}

/** ValidateUserMpTactics

    Validate all user tactics. Call it right after saved user tactics are loaded.
    Return true if tactics are OK, false if there had to be some changes.
*/
bool ValidateUserMpTactics()
{
    bool modified = true;

    for (size_t i = 0; i < sizeofarray(MP_Tactics); i++) {
        if (!ValidateUserMpTactic(MP_Tactics + i)) {
            InitMpTactic(MP_Tactics + i, tacticsStringTable[TACTICS_USER_A + i]);
            modified = false;
        }
    }

    return modified;
}

static char *LoadTeam()
{
    auto teamId = GetCurrentTeamId();
    int teamNo = teamId & 0xff, ordinal = teamId >> 8 & 0xff;
    D0 = teamNo;
    calla_ebp_safe(LoadTeamFile);
    assert(D0);
    return teamFileBuffer + 2 + ordinal * sizeof(TeamFile);
}

void InitializeMPOptionsMenu()
{
    /* stoopid DrawSprite16Pixels has hard-coded width of 384, patch it temporarily so we can draw player icons */
    static const byte patchDrawSprite[9] = { 0xb8, 0x40, 0x01, 0x00, 0x00, 0xf7, 0xe7, 0x8b, 0xf8 };
    memcpy((char *)SWOS_DrawSprite16Pixels + 0x116, patchDrawSprite, sizeof(patchDrawSprite));
    MPOptionsMenuAfterDraw();
}

void MPOptionsMenuAfterDraw()
{
    D0 = 1;
    calla_ebp_safe(CalcMenuEntryAddress);
    strcpy(((MenuEntry *)A0)->u2.string, GetPlayerNick());
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
    auto timeout = SetNetworkTimeout(GetNetworkTimeout() + 70);
    UpdateNetworkTimeout(timeout);
}

void DecreaseNetworkTimeout()
{
    auto timeout = SetNetworkTimeout(GetNetworkTimeout() - 70);
    UpdateNetworkTimeout(timeout);
}

void IncreaseSkipFrames()
{
    auto frames =  SetSkipFrames(GetSkipFrames() + 1);
    UpdateSkipFrames(frames);
}

void DecreaseSkipFrames()
{
    auto frames = SetSkipFrames(GetSkipFrames() - 1);
    UpdateSkipFrames(frames);
}

extern char aTeamNotChosen[] asm("aTeamNotChosen");

void mpOptSelectTeamBeforeDraw()
{
    MenuEntry *entry = (MenuEntry *)A5;
    entry->u2.string = GetCurrentTeamId() == (dword)-1 ? aTeamNotChosen : LoadTeam() + 5;
}

/** ChooseMPTactics

    Invoked when user selects a user tactic to edit in multiplayer options menu.
*/
void ChooseMPTactics()
{
    MenuEntry *entry = (MenuEntry *)A5;
    if (GetCurrentTeamId() == (dword)-1) {
        A0 = (dword)"PLEASE CHOOSE A TEAM FIRST";
        calla(ShowErrorMenu);
        return;
    }

    int tacticsIndex = (entry->u2.string - (char *)MP_Tactics) / TACTIC_SIZE;
    assert(tacticsIndex >= 0 && tacticsIndex < 6);

    chosenTactics = TACTICS_USER_A + tacticsIndex;
    WriteToLog("Editing tactics: %s", entry->u2.string);

    /* preserve user tactics, and plant ours for the editing */
    auto userTactics = USER_A + tacticsIndex;
    Tactics savedTactics = *userTactics;
    *userTactics = MP_Tactics[tacticsIndex];

    chooseTacticsTeamPtr = LoadTeam();
    calla(InitLittlePlayerSprites);
    A6 = (dword)editTacticsMenu;
    calla(ShowMenu);

    MP_Tactics[tacticsIndex] = *editTacticsCurrentTactics;
    /* restore non-multiplayer tactics as if nothing happened */
    *userTactics = savedTactics;
    WriteToLog("Finished editing tactics: %s", editTacticsCurrentTactics);
}

void ExitMultiplayerOptions()
{
    /* unpatch sprite drawing */
    static const byte unpatchDrawSprite[9] = { 0xc1, 0xe7, 0x07, 0x8b, 0xc7, 0xd1, 0xe7, 0x03, 0xf8 };
    memcpy((char *)SWOS_DrawSprite16Pixels + 0x116, unpatchDrawSprite, sizeof(unpatchDrawSprite));
    SaveOptionsIfNeeded();
    calla_ebp_safe(SetExitMenuFlag);
}
