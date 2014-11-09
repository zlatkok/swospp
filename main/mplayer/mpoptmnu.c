/** mpoptmnu.c

    Multiplayer options menu event handlers.
*/

#include "swos.h"
#include "util.h"
#include "dosipx.h"
#include "mplayer.h"
#include "options.h"

static Tactics mpTactics[6];

void RegisterUserTactics(RegisterOptionsFunc registerOptions)
{
    registerOptions("tactics", 7, "User tactics in multiplayer games", 33, "%n"
        "%370b/USER_A" "%370b/USER_B" "%370b/USER_C"
        "%370b/USER_D" "%370b/USER_E" "%370b/USER_F",
        &mpTactics[0], &mpTactics[1], &mpTactics[2], &mpTactics[3], &mpTactics[4], &mpTactics[5]);
}

/**

    Validate single user tactic, return true if it passes.
*/
static bool validateUserTactics(const Tactics *tactics)
{
    char c = 0;
    int i;
    /* check if name is all zeros */
    for (i = 0; i < member_size(Tactics, name); i++)
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

/**

    Validate all user tactics. Call it right after saved user tactics are loaded.
    Return true if tactics are ok, false if there had to be some changes.
*/
bool ValidateUserTactics()
{
    int i;
    bool modified = true;
    for (i = 0; i < sizeofarray(mpTactics); i++)
        if (!validateUserTactics(mpTactics + i)) {
            /* if nul tactics just put 4-4-2 default tactics, just like SWOS does */
            mpTactics[i] = *tact_4_4_2;
            modified = false;
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

extern char aTeamNotChosen[];
#pragma aux aTeamNotChosen "*";

void mpOptSelectTeamBeforeDraw()
{
    MenuEntry *entry = (MenuEntry *)A5;
    entry->u2.string = currentTeamId == -1 ? aTeamNotChosen : LoadTeam() + 5;
}

void ChooseMPTactics()
{
    int tacticsIndex;
    MenuEntry *entry = (MenuEntry *)A5;
    if (currentTeamId == -1) {
        A0 = (dword)"PLEASE CHOOSE A TEAM FIRST";
        calla(ShowErrorMenu);
        return;
    }
    tacticsIndex = (entry->u2.string - (char *)USER_A) / TACTICS_SIZE;
    assert(tacticsIndex >= 0 && tacticsIndex < 6);
    chosenTactics = TACTICS_USER_A + tacticsIndex;
    WriteToLog(("Editing tactics %s", entry->u2.string));
    chooseTacticsTeamPtr = LoadTeam();
    WriteToLog(("Team ptr = %#x", chooseTacticsTeamPtr));
    calla(InitLittlePlayerSprites);
    A6 = (dword)editTacticsMenu;
    calla(ShowMenu);
    mpTactics[tacticsIndex] = *editTacticsCurrentTactics;
}

void ExitMultiplayerOptions()
{
    /* unpatch sprite drawing */
    static const byte unpatchDrawSprite[9] = { 0xc1, 0xe7, 0x07, 0x8b, 0xc7, 0xd1, 0xe7, 0x03, 0xf8 };
    memcpy((char *)SWOS_DrawSprite16Pixels + 0x116, unpatchDrawSprite, sizeof(unpatchDrawSprite));
    SaveOptionsIfNeeded();
    calla(SetExitMenuFlag);
}