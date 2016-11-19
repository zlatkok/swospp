/*
   swossym.h - SWOS symbols header file

   Automatically generated from swos.map - do not edit
*/

#pragma once
extern "C" {

extern dword swosCodeBase;
extern byte prevVideoMode asm ("prevVideoMode");
extern word EGA_graphics asm ("EGA_graphics");
extern void (*FindFiles[])() asm ("FindFiles");
extern void (*GameLoop[])() asm ("GameLoop");
extern void (*Flip[])() asm ("Flip");
extern void (*SWOS_EndProgram[])() asm ("SWOS_EndProgram");
extern Menu *currentMenu asm ("currentMenu");
extern void (*ShowMenu[])() asm ("ShowMenu");
extern void (*CalcMenuEntryAddress[])() asm ("CalcMenuEntryAddress");
extern word controlWord asm ("controlWord");
extern word joy1Status asm ("joy1Status");
extern word joy2Status asm ("joy2Status");
extern void (*Joy2SetStatus[])() asm ("Joy2SetStatus");
extern void (*Player1StatusProc[])() asm ("Player1StatusProc");
extern void (*Player2StatusProc[])() asm ("Player2StatusProc");
extern char smallCharsTable[] asm ("smallCharsTable");
extern void (*DrawMenuText[])() asm ("DrawMenuText");
extern void (*SWOS_DrawSprite[])() asm ("SWOS_DrawSprite");
extern char *lin_adr_384k asm ("lin_adr_384k");
extern void (*ShowErrorMenu[])() asm ("ShowErrorMenu");
extern char aExit[] asm ("aExit");
extern void (*SetExitMenuFlag[])() asm ("SetExitMenuFlag");
/*
    controls
*/
extern word numLoopsJoy1 asm ("numLoopsJoy1");
extern word numLoopsJoy2 asm ("numLoopsJoy2");
/*
    SWOS original options
*/
extern word gameLength asm ("gameLength");
extern word autoReplays asm ("autoReplays");
extern word menuMusic asm ("menuMusic");
extern word autoSaveHighlights asm ("autoSaveHighlights");
extern word allPlayerTeamsEqual asm ("allPlayerTeamsEqual");
extern word pitchType asm ("pitchType");
extern word commentary asm ("commentary");
extern word chairmanScenes asm ("chairmanScenes");
extern word joyKbdWord asm ("joyKbdWord");
extern short sineCosineTable[256] asm ("sineCosineTable");
extern char *vsPtr asm ("vsPtr");
/*
    68k register set
*/
extern dword D0 asm ("D0");
extern dword D1 asm ("D1");
extern dword D2 asm ("D2");
extern dword D3 asm ("D3");
extern dword D4 asm ("D4");
extern dword D5 asm ("D5");
extern dword D6 asm ("D6");
extern dword D7 asm ("D7");
extern dword A0 asm ("A0");
extern dword A1 asm ("A1");
extern dword A2 asm ("A2");
extern dword A3 asm ("A3");
extern dword A4 asm ("A4");
extern dword A5 asm ("A5");
extern dword A6 asm ("A6");
extern void (*SaveSprite[])() asm ("SaveSprite");
extern SpriteGraphics *spritesIndex[] asm ("spritesIndex");
extern TeamGeneralInfo *benchTeam asm ("benchTeam");
extern void (*SetCurrentEntry[])() asm ("SetCurrentEntry");
extern void (*DrawMenu[])() asm ("DrawMenu");
extern void (*LoadDIYFile[])() asm ("LoadDIYFile");
extern void (*PrimitivePrintf[])() asm ("PrimitivePrintf");
extern word numSelectedTeams asm ("numSelectedTeams");
extern byte selectedTeamsBuffer[] asm ("selectedTeamsBuffer");
extern word gameType asm ("gameType");
extern char pitchDatBuffer[] asm ("pitchDatBuffer");
extern word inSubstitutesMenu asm ("inSubstitutesMenu");
extern void (*MainKeysCheck[])() asm ("MainKeysCheck");
extern word lastKey asm ("lastKey");
extern void (*VerifyJoypadControls[])() asm ("VerifyJoypadControls");
extern volatile ushort currentTick asm ("currentTick");
extern byte seed asm ("seed");
extern byte seed2 asm ("seed2");
extern void (*Rand[])() asm ("Rand");
extern void (*SaveOptions[])() asm ("SaveOptions");
extern void (*RestoreOptions[])() asm ("RestoreOptions");
extern void (*DrawMenuItem[])() asm ("DrawMenuItem");
extern void (*ReadGamePort[])() asm ("ReadGamePort");
extern char bigCharsTable[] asm ("bigCharsTable");
extern void (*GetKey[])() asm ("GetKey");
extern char aDataTeam_nnn[] asm ("aDataTeam_nnn");
extern char aCustoms_edt[] asm ("aCustoms_edt");
/*
    custom tactics
*/
extern Tactics USER_A[] asm ("USER_A");
extern Tactics USER_B[] asm ("USER_B");
extern Tactics USER_C[] asm ("USER_C");
extern Tactics USER_D[] asm ("USER_D");
extern Tactics USER_E[] asm ("USER_E");
extern Tactics USER_F[] asm ("USER_F");
extern void (*Joy1SetStatus[])() asm ("Joy1SetStatus");
extern word longFireFlag asm ("longFireFlag");
extern void (*main_loop[])() asm ("main_loop");
extern void (*ReadTimerDelta[])() asm ("ReadTimerDelta");
extern word teamSwitchCounter asm ("teamSwitchCounter");
extern void (*InitTeamsData[])() asm ("InitTeamsData");
extern word left_team_player_no asm ("left_team_player_no");
extern word left_team_coach_no asm ("left_team_coach_no");
extern word right_team_player_no asm ("right_team_player_no");
extern word right_team_coach_no asm ("right_team_coach_no");
extern TeamGeneralInfo leftTeamData[] asm ("leftTeamData");
extern TeamGeneralInfo rightTeamData[] asm ("rightTeamData");
extern void (*SetupPlayers[])() asm ("SetupPlayers");
extern word random_seed asm ("random_seed");
extern volatile word stoppageTimer asm ("stoppageTimer");
extern void (*TimerProc[])() asm ("TimerProc");
extern void (*CalculateIfPlayerWinsBall[])() asm ("CalculateIfPlayerWinsBall");
extern void (*UpdateStatistics[])() asm ("UpdateStatistics");
extern word paused asm ("paused");
extern word showStats asm ("showStats");
extern word showingStats asm ("showingStats");
extern void (*ClearBackground[])() asm ("ClearBackground");
extern void (*AllowStatistics[])() asm ("AllowStatistics");
extern void (*StatisticsOff[])() asm ("StatisticsOff");
extern void (*TeamsControlsCheck[])() asm ("TeamsControlsCheck");
extern word gameState asm ("gameState");
extern word gameStatePl asm ("gameStatePl");
extern word replayState asm ("replayState");
extern word bench1Called asm ("bench1Called");
extern word bench2Called asm ("bench2Called");
extern word halfNumber asm ("halfNumber");
extern word replaySelected asm ("replaySelected");
extern word playGame asm ("playGame");
extern void (*AIL_stop_play[])() asm ("AIL_stop_play");
extern word menuFade asm ("menuFade");
extern word teamPlayingUp asm ("teamPlayingUp");
extern word pl1LastFired asm ("pl1LastFired");
extern word pl2LastFired asm ("pl2LastFired");
extern word pl1ShortFireCounter asm ("pl1ShortFireCounter");
extern word pl2ShortFireCounter asm ("pl2ShortFireCounter");
extern word gameStoppedTimer asm ("gameStoppedTimer");
extern word goalCounter asm ("goalCounter");
extern word stateGoal asm ("stateGoal");
extern word benchCounter asm ("benchCounter");
extern word benchCounter2 asm ("benchCounter2");
extern word longFireCounter asm ("longFireCounter");
extern word longFireTime asm ("longFireTime");
extern Sprite ballSprite[] asm ("ballSprite");
extern word frameCount asm ("frameCount");
extern void (*DrawSprites[])() asm ("DrawSprites");
extern word numSpritesToRender asm ("numSpritesToRender");
extern Sprite *allSpritesArray[] asm ("allSpritesArray");
extern Sprite *sortedSprites[] asm ("sortedSprites");
extern void (*SWOS_DrawSprite16Pixels[])() asm ("SWOS_DrawSprite16Pixels");
extern byte deltaColor asm ("deltaColor");
extern void (*DarkenRectangle[])() asm ("DarkenRectangle");
extern short cameraX asm ("cameraX");
extern short cameraY asm ("cameraY");
extern void (*ScrollToCurrent[])() asm ("ScrollToCurrent");
extern dword gameTime asm ("gameTime");
extern void (*CopySprite[])() asm ("CopySprite");
extern char *selTeamsPtr asm ("selTeamsPtr");
extern SWOS_ScorerInfo team1Scorers[8] asm ("team1Scorers");
extern SWOS_ScorerInfo team2Scorers[8] asm ("team2Scorers");
extern word cameraXFraction asm ("cameraXFraction");
extern word cameraYFraction asm ("cameraYFraction");
extern Sprite team1Scorer1Sprite[] asm ("team1Scorer1Sprite");
extern Sprite team2Scorer1Sprite[] asm ("team2Scorer1Sprite");
extern void (*ClearSprite[])() asm ("ClearSprite");
extern TeamGame *leftTeamPtr asm ("leftTeamPtr");
extern TeamGame *rightTeamPtr asm ("rightTeamPtr");
extern char *alreadyScoredTable[3] asm ("alreadyScoredTable");
extern char *firstGoalScoredTable[3] asm ("firstGoalScoredTable");
extern void (*SWOS_Text2Sprite[])() asm ("SWOS_Text2Sprite");
extern word animPatternsState asm ("animPatternsState");
extern void (*DrawAnimatedPatterns[])() asm ("DrawAnimatedPatterns");
extern TeamStatsData team1StatsData[] asm ("team1StatsData");
extern void (*DrawStatistics[])() asm ("DrawStatistics");
extern byte *advert1Pointers[8] asm ("advert1Pointers");
extern byte *advert2Pointers[8] asm ("advert2Pointers");
extern byte *advert3Pointers[8] asm ("advert3Pointers");
extern word statsTimeout asm ("statsTimeout");
extern word eventTimer asm ("eventTimer");
extern void (*DrawSpriteInColor[])() asm ("DrawSpriteInColor");
extern void (*DrawBenchAndSubsMenu[])() asm ("DrawBenchAndSubsMenu");
extern void (*DrawBenchPlayersAndCoach[])() asm ("DrawBenchPlayersAndCoach");
extern void (*SWOS_DrawSpriteInGame[])() asm ("SWOS_DrawSpriteInGame");
extern void (*DrawSubstitutesMenu[])() asm ("DrawSubstitutesMenu");
extern void (*DrawFormationMenu[])() asm ("DrawFormationMenu");
extern void (*SubstitutePlayerIfFiring[])() asm ("SubstitutePlayerIfFiring");
extern void (*EnqueueSubstituteSample[])() asm ("EnqueueSubstituteSample");
extern short subsState asm ("subsState");
extern TeamGame *currentSubsTeam asm ("currentSubsTeam");
extern short plToBeSubstitutedPos asm ("plToBeSubstitutedPos");
extern short plToEnterGameIndex asm ("plToEnterGameIndex");
extern word selectedFormationEntry asm ("selectedFormationEntry");
extern word subsMenuX asm ("subsMenuX");
extern short plToBeSubstitutedOrd asm ("plToBeSubstitutedOrd");
extern short selectedPlayerInSubs asm ("selectedPlayerInSubs");
extern word formationMenuX asm ("formationMenuX");
extern void (*LoadTeamFile[])() asm ("LoadTeamFile");
extern char teamFileBuffer[] asm ("teamFileBuffer");
extern void (*InitLittlePlayerSprites[])() asm ("InitLittlePlayerSprites");
extern char editTacticsMenu[] asm ("editTacticsMenu");
extern short chosenTactics asm ("chosenTactics");
extern char *chooseTacticsTeamPtr asm ("chooseTacticsTeamPtr");
extern void (*DrawSpriteCentered[])() asm ("DrawSpriteCentered");
extern void (*DrawLittlePlayersAndBall[])() asm ("DrawLittlePlayersAndBall");
extern word pl1Tactics asm ("pl1Tactics");
extern word pl2Tactics asm ("pl2Tactics");
extern Tactics *tacticsTable[] asm ("tacticsTable");
extern void (*ShowFormationMenu[])() asm ("ShowFormationMenu");
extern void (*ChangeFormationIfFiring[])() asm ("ChangeFormationIfFiring");
extern void (*CheckContinueAbortPrompt[])() asm ("CheckContinueAbortPrompt");
extern char saveFileTitle[] asm ("saveFileTitle");
extern char saveFileName[] asm ("saveFileName");
extern void (*SaveCareerSelected[])() asm ("SaveCareerSelected");
extern void (*StartContest[])() asm ("StartContest");
extern void (*PrepareMenu[])() asm ("PrepareMenu");
extern char SWOS_MainMenu[] asm ("SWOS_MainMenu");
extern TeamFile *playMatchTeam1Ptr asm ("playMatchTeam1Ptr");
extern TeamFile *playMatchTeam2Ptr asm ("playMatchTeam2Ptr");
extern Tactics tact_4_4_2[] asm ("tact_4_4_2");
extern Tactics editTacticsCurrentTactics[] asm ("editTacticsCurrentTactics");
extern char *tacticsStringTable[] asm ("tacticsStringTable");
extern void (*InitializeTacticsPositions[])() asm ("InitializeTacticsPositions");
extern void (*InitMainMenuStuff[])() asm ("InitMainMenuStuff");
extern const char hexDigits[] asm ("hexDigits");
extern char continueMenu[] asm ("continueMenu");
extern const char aTeam2_dat[] asm ("aTeam2_dat");
extern void (*LoadFile[])() asm ("LoadFile");
extern void (*UnchainSpriteInMenus[])() asm ("UnchainSpriteInMenus");
extern void (*FillSkinColorConversionTable[])() asm ("FillSkinColorConversionTable");
extern void (*ConvertSpriteColors[])() asm ("ConvertSpriteColors");
extern char editTeamsSaveVarsArea[] asm ("editTeamsSaveVarsArea");
extern void (*DoUnchainSpriteInMenus[])() asm ("DoUnchainSpriteInMenus");
extern const short angleCoeficients[32][32] asm ("angleCoeficients");
extern const short defaultPlayerDestinations[8][2] asm ("defaultPlayerDestinations");
extern void (*MovePlayer[])() asm ("MovePlayer");
extern const char playerRunningAnimTable[] asm ("playerRunningAnimTable");
extern void (*SetAnimationTable[])() asm ("SetAnimationTable");
extern void (*CalculateDeltaXAndY[])() asm ("CalculateDeltaXAndY");
extern void (*SetNextPlayerFrame[])() asm ("SetNextPlayerFrame");
extern const word playerSpeedsGameInProgress[8] asm ("playerSpeedsGameInProgress");
extern word goalScored asm ("goalScored");
extern const char playerNormalStandingAnimTable[] asm ("playerNormalStandingAnimTable");
/*
    Watcom C library
*/
extern void (*swos_libc_stackavail_[])() asm ("swos_libc_stackavail_");
extern void (*swos_libc_strncpy_[])() asm ("swos_libc_strncpy_");
extern void (*swos_libc_strlen_[])() asm ("swos_libc_strlen_");
extern void (*swos_libc_strcmp_[])() asm ("swos_libc_strcmp_");
extern void (*swos_libc_strncmp_[])() asm ("swos_libc_strncmp_");
extern void (*swos_libc_strnicmp_[])() asm ("swos_libc_strnicmp_");
extern void (*swos_libc_int386x_[])() asm ("swos_libc_int386x_");
extern void (*swos_libc_segread_[])() asm ("swos_libc_segread_");
extern void (*swos_libc_exit_[])() asm ("swos_libc_exit_");
extern void (*swos_libc_memset_[])() asm ("swos_libc_memset_");
extern void (*swos_libc_memmove_[])() asm ("swos_libc_memmove_");
extern void (*swos_libc_time_[])() asm ("swos_libc_time_");
extern void (*swos_libc_ctime_[])() asm ("swos_libc_ctime_");

}
