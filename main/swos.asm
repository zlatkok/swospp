; Automatically generated from swos.map - do not edit

[list -]

section .swos bss

global swosCodeBase
swosCodeBase:

global SWOS
times 0x5758-($-$$) resb 1
SWOS:

global GameLoop
times 0x5a0d-($-$$) resb 1
GameLoop:

global main_loop
times 0x5d94-($-$$) resb 1
main_loop:

global Rand
times 0x60de-($-$$) resb 1
Rand:

global MainKeysCheck
times 0x6176-($-$$) resb 1
MainKeysCheck:

global EnqueueSubstituteSample
times 0x8725-($-$$) resb 1
EnqueueSubstituteSample:

global Initialization
times 0x92be-($-$$) resb 1
Initialization:

global TimerProc
times 0x9819-($-$$) resb 1
TimerProc:

global Int9KeyboardHandler
times 0x9870-($-$$) resb 1
Int9KeyboardHandler:

global SetPrevVideoModeEndProgram
times 0x991a-($-$$) resb 1
SetPrevVideoModeEndProgram:

global SWOS_EndProgram
times 0x9923-($-$$) resb 1
SWOS_EndProgram:

global ProgramPIT
times 0x995f-($-$$) resb 1
ProgramPIT:

global WaitRetrace
times 0x9977-($-$$) resb 1
WaitRetrace:

global VerifyJoypadControls
times 0x9a0c-($-$$) resb 1
VerifyJoypadControls:

global ReadGamePort
times 0x9a72-($-$$) resb 1
ReadGamePort:

global Joy1SetStatus
times 0x9ae2-($-$$) resb 1
Joy1SetStatus:

global Joy2SetStatus
times 0x9c5d-($-$$) resb 1
Joy2SetStatus:

global Player1StatusProc
times 0x9dd7-($-$$) resb 1
Player1StatusProc:

global Player2StatusProc
times 0x9f96-($-$$) resb 1
Player2StatusProc:

global WriteFile
times 0xa155-($-$$) resb 1
WriteFile:

global FlipInMenu
times 0xa250-($-$$) resb 1
FlipInMenu:

global SetControlWord
times 0xa2b3-($-$$) resb 1
SetControlWord:

global GetKey
times 0xa6ae-($-$$) resb 1
GetKey:

global DumpTimerVariables
times 0xa929-($-$$) resb 1
DumpTimerVariables:

global RestoreOldInt9Handler
times 0xad76-($-$$) resb 1
RestoreOldInt9Handler:

global DumpScreen
times 0xad91-($-$$) resb 1
DumpScreen:

global CDROM_StopAudio
times 0xb1e5-($-$$) resb 1
CDROM_StopAudio:

global SWOS_Text2Sprite
times 0xbc61-($-$$) resb 1
SWOS_Text2Sprite:

global CopySprite
times 0xc68b-($-$$) resb 1
CopySprite:

global DrawSpriteCentered
times 0xc76d-($-$$) resb 1
DrawSpriteCentered:

global SWOS_DrawSprite
times 0xc7e2-($-$$) resb 1
SWOS_DrawSprite:

global ClearSprite
times 0xc964-($-$$) resb 1
ClearSprite:

global SWOS_DrawSpriteInGame
times 0xcbaa-($-$$) resb 1
SWOS_DrawSpriteInGame:

global DrawSpriteInColor
times 0xcc4a-($-$$) resb 1
DrawSpriteInColor:

global DarkenRectangle
times 0xcde0-($-$$) resb 1
DarkenRectangle:

global SWOS_DrawSprite16Pixels
times 0xcf3e-($-$$) resb 1
SWOS_DrawSprite16Pixels:

global SaveSprite
times 0xd238-($-$$) resb 1
SaveSprite:

global ClearBackground
times 0xd2cb-($-$$) resb 1
ClearBackground:

global ShowMenu
times 0xd672-($-$$) resb 1
ShowMenu:

global CallAfterDraw
times 0xd6f3-($-$$) resb 1
CallAfterDraw:

global SetCurrentEntry
times 0xd782-($-$$) resb 1
SetCurrentEntry:

global SetExitMenuFlag
times 0xd7a0-($-$$) resb 1
SetExitMenuFlag:

global ReadTimerDelta
times 0xd8cb-($-$$) resb 1
ReadTimerDelta:

global PrepareMenu
times 0xdae7-($-$$) resb 1
PrepareMenu:

global CalcMenuEntryAddress
times 0xdb81-($-$$) resb 1
CalcMenuEntryAddress:

global DrawMenu
times 0xdc06-($-$$) resb 1
DrawMenu:

global MenuProc
times 0xddab-($-$$) resb 1
MenuProc:

global DrawMenuItem
times 0xe341-($-$$) resb 1
DrawMenuItem:

global DrawMenuFrame
times 0xef39-($-$$) resb 1
DrawMenuFrame:

global DrawMenuBack
times 0xf12e-($-$$) resb 1
DrawMenuBack:

global DrawMenuText
times 0xf1ce-($-$$) resb 1
DrawMenuText:

global DrawMenuTextCentered
times 0xf3ff-($-$$) resb 1
DrawMenuTextCentered:

global GetTextSize
times 0xf455-($-$$) resb 1
GetTextSize:

global PrimitivePrintf
times 0xf788-($-$$) resb 1
PrimitivePrintf:

global Int2Ascii
times 0x104bf-($-$$) resb 1
Int2Ascii:

global DrawOuterFrame
times 0x10620-($-$$) resb 1
DrawOuterFrame:

global CheckControls
times 0x10829-($-$$) resb 1
CheckControls:

global InputText
times 0x10cca-($-$$) resb 1
InputText:

global FilterKeys
times 0x10fef-($-$$) resb 1
FilterKeys:

global ChangePitchType
times 0x1181f-($-$$) resb 1
ChangePitchType:

global ChangeGameLength
times 0x1183f-($-$$) resb 1
ChangeGameLength:

global SetDefaultOptions
times 0x11943-($-$$) resb 1
SetDefaultOptions:

global SaveOptions
times 0x119b2-($-$$) resb 1
SaveOptions:

global RestoreOptions
times 0x119f9-($-$$) resb 1
RestoreOptions:

global SWOSMainMenuInit
times 0x12204-($-$$) resb 1
SWOSMainMenuInit:

global CommonMenuExit
times 0x1221a-($-$$) resb 1
CommonMenuExit:

global StartContest
times 0x12737-($-$$) resb 1
StartContest:

global SaveCareerSelected
times 0x12af0-($-$$) resb 1
SaveCareerSelected:

global InitAndPlayGame
times 0x1390e-($-$$) resb 1
InitAndPlayGame:

global SetupPlayers
times 0x13bab-($-$$) resb 1
SetupPlayers:

global CheckContinueAbortPrompt
times 0x14cd4-($-$$) resb 1
CheckContinueAbortPrompt:

global LoadTeamFile
times 0x229c3-($-$$) resb 1
LoadTeamFile:

global LoadDIYFile
times 0x2333f-($-$$) resb 1
LoadDIYFile:

global SetName
times 0x24187-($-$$) resb 1
SetName:

global PlayMatchAfterDraw
times 0x2f71f-($-$$) resb 1
PlayMatchAfterDraw:

global ExitPlayMatch
times 0x300b6-($-$$) resb 1
ExitPlayMatch:

global PlayMatchSelected
times 0x30109-($-$$) resb 1
PlayMatchSelected:

global ShowErrorMenu
times 0x32777-($-$$) resb 1
ShowErrorMenu:

global DoContinueAbortMenu
times 0x32791-($-$$) resb 1
DoContinueAbortMenu:

global ExitChooseTeams
times 0x3a4b3-($-$$) resb 1
ExitChooseTeams:

global ChooseTeamsDialog
times 0x3c758-($-$$) resb 1
ChooseTeamsDialog:

global DrawLittlePlayersAndBall
times 0x411cf-($-$$) resb 1
DrawLittlePlayersAndBall:

global InitLittlePlayerSprites
times 0x420fc-($-$$) resb 1
InitLittlePlayerSprites:

global SetBenchPlayersNumbers
times 0x42528-($-$$) resb 1
SetBenchPlayersNumbers:

global DrawAnimatedPatterns
times 0x53018-($-$$) resb 1
DrawAnimatedPatterns:

global ScrollToCurrent
times 0x57863-($-$$) resb 1
ScrollToCurrent:

global AllowStatistics
times 0x581e4-($-$$) resb 1
AllowStatistics:

global StatisticsOff
times 0x581fe-($-$$) resb 1
StatisticsOff:

global UpdateStatistics
times 0x58211-($-$$) resb 1
UpdateStatistics:

global DrawStatistics
times 0x584e2-($-$$) resb 1
DrawStatistics:

global BenchCheckControls
times 0x59389-($-$$) resb 1
BenchCheckControls:

global ShowFormationMenu
times 0x5a052-($-$$) resb 1
ShowFormationMenu:

global SubstitutePlayerIfFiring
times 0x5a0a4-($-$$) resb 1
SubstitutePlayerIfFiring:

global ChangeFormationIfFiring
times 0x5a996-($-$$) resb 1
ChangeFormationIfFiring:

global DrawBenchAndSubsMenu
times 0x5b213-($-$$) resb 1
DrawBenchAndSubsMenu:

global DrawBenchPlayersAndCoach
times 0x5b49a-($-$$) resb 1
DrawBenchPlayersAndCoach:

global DrawSubstitutesMenu
times 0x5b89e-($-$$) resb 1
DrawSubstitutesMenu:

global DrawSubstitutesMenuEntry
times 0x5be61-($-$$) resb 1
DrawSubstitutesMenuEntry:

global DrawFormationMenu
times 0x5c7f3-($-$$) resb 1
DrawFormationMenu:

global SaveCoordinatesForHighlights
times 0x5cf42-($-$$) resb 1
SaveCoordinatesForHighlights:

global IncrementHilBufferPointer
times 0x5d0ac-($-$$) resb 1
IncrementHilBufferPointer:

global ValidateHilPointerAdd
times 0x5d9ed-($-$$) resb 1
ValidateHilPointerAdd:

global SetHilFileHeader
times 0x5dc12-($-$$) resb 1
SetHilFileHeader:

global DrawControlledPlayerNumbers
times 0x5f6e3-($-$$) resb 1
DrawControlledPlayerNumbers:

global BookPlayer
times 0x611d0-($-$$) resb 1
BookPlayer:

global DrawSprites
times 0x62270-($-$$) resb 1
DrawSprites:

global InitTeamsData
times 0x62548-($-$$) resb 1
InitTeamsData:

global TeamsControlsCheck
times 0x62abb-($-$$) resb 1
TeamsControlsCheck:

global CalculateIfPlayerWinsBall
times 0x6a14a-($-$$) resb 1
CalculateIfPlayerWinsBall:

global GetFilenameAndExtension
times 0x75c50-($-$$) resb 1
GetFilenameAndExtension:

global FindFiles
times 0x75e5b-($-$$) resb 1
FindFiles:

global SelFilesBeforeDrawCommon
times 0x764d8-($-$$) resb 1
SelFilesBeforeDrawCommon:

global SelectFileToSaveDialog
times 0x76adf-($-$$) resb 1
SelectFileToSaveDialog:

global ShowHighlights
times 0x76ecf-($-$$) resb 1
ShowHighlights:

global Flip
times 0x86486-($-$$) resb 1
Flip:

global StopAIL
times 0x8ad26-($-$$) resb 1
StopAIL:

global AIL_stop_play
times 0x8b1b7-($-$$) resb 1
AIL_stop_play:

global swos_start
times 0x8ca5c-($-$$) resb 1
swos_start:

global swos_libc_strcmp_
times 0x92f80-($-$$) resb 1
swos_libc_strcmp_:

global swos_libc_memset_
times 0x93050-($-$$) resb 1
swos_libc_memset_:

global swos_libc_time_
times 0x940a7-($-$$) resb 1
swos_libc_time_:

global swos_libc_ctime_
times 0x9426d-($-$$) resb 1
swos_libc_ctime_:

global swos_libc_strlen_
times 0x9a781-($-$$) resb 1
swos_libc_strlen_:

global swos_libc_strnicmp_
times 0x9a79a-($-$$) resb 1
swos_libc_strnicmp_:

global swos_libc_memmove_
times 0x9a856-($-$$) resb 1
swos_libc_memmove_:

global swos_libc_strncpy_
times 0x9a8a3-($-$$) resb 1
swos_libc_strncpy_:

global swos_libc_segread_
times 0x9b1e2-($-$$) resb 1
swos_libc_segread_:

global swos_libc_int386x_
times 0x9b20a-($-$$) resb 1
swos_libc_int386x_:

global swos_libc_stackavail_
times 0x9b303-($-$$) resb 1
swos_libc_stackavail_:

global swos_libc_exit_
times 0x9b3be-($-$$) resb 1
swos_libc_exit_:

global swos_libc_strncmp_
times 0x9e499-($-$$) resb 1
swos_libc_strncmp_:


global swosDataBase
times 0xb0000-($-$$) resb 1
swosDataBase:

global aPlayer_0
times 0xb25bf-($-$$) resb 1
aPlayer_0:

global aPlayerCoach_0
times 0xb25c6-($-$$) resb 1
aPlayerCoach_0:

global aCoach_3
times 0xb25d3-($-$$) resb 1
aCoach_3:

global aHigh
times 0xb2895-($-$$) resb 1
aHigh:

global aReplay
times 0xb45fe-($-$$) resb 1
aReplay:

global aComputer
times 0xb4646-($-$$) resb 1
aComputer:

global aContinue
times 0xb4892-($-$$) resb 1
aContinue:

global aOn
times 0xb48a3-($-$$) resb 1
aOn:

global aOff
times 0xb48ac-($-$$) resb 1
aOff:

global aOk
times 0xb48b5-($-$$) resb 1
aOk:

global aExit
times 0xb48cb-($-$$) resb 1
aExit:

global aAbort
times 0xb49bb-($-$$) resb 1
aAbort:

global aPlus
times 0xb49da-($-$$) resb 1
aPlus:

global aMinus
times 0xb49dc-($-$$) resb 1
aMinus:

global aSubstitutes
times 0xb4bf6-($-$$) resb 1
aSubstitutes:

global aFrom
times 0xb4c02-($-$$) resb 1
aFrom:

global aPitchType
times 0xb4ca9-($-$$) resb 1
aPitchType:

global aGameLength
times 0xb5236-($-$$) resb 1
aGameLength:

global currentTeamNumber
times 0xc8b66-($-$$) resb 1
currentTeamNumber:

global gameType
times 0xd004e-($-$$) resb 1
gameType:

global USER_A
times 0xd0054-($-$$) resb 1
USER_A:

global USER_B
times 0xd01c6-($-$$) resb 1
USER_B:

global USER_C
times 0xd0338-($-$$) resb 1
USER_C:

global USER_D
times 0xd04aa-($-$$) resb 1
USER_D:

global USER_E
times 0xd061c-($-$$) resb 1
USER_E:

global USER_F
times 0xd078e-($-$$) resb 1
USER_F:

global gameLength
times 0xd0900-($-$$) resb 1
gameLength:

global autoReplays
times 0xd0902-($-$$) resb 1
autoReplays:

global menuMusic
times 0xd0904-($-$$) resb 1
menuMusic:

global autoSaveHighlights
times 0xd0906-($-$$) resb 1
autoSaveHighlights:

global allPlayerTeamsEqual
times 0xd0908-($-$$) resb 1
allPlayerTeamsEqual:

global pitchType
times 0xd090a-($-$$) resb 1
pitchType:

global commentary
times 0xd090c-($-$$) resb 1
commentary:

global chairmanScenes
times 0xd090e-($-$$) resb 1
chairmanScenes:

global seed2
times 0xd0910-($-$$) resb 1
seed2:

global random_seed
times 0xd0914-($-$$) resb 1
random_seed:

global selTeamsPtr
times 0xd0928-($-$$) resb 1
selTeamsPtr:

global numSelectedTeams
times 0xd093b-($-$$) resb 1
numSelectedTeams:

global selectedTeamsBuffer
times 0xd093d-($-$$) resb 1
selectedTeamsBuffer:

global D0
times 0xe146d-($-$$) resb 1
D0:

global D1
times 0xe1471-($-$$) resb 1
D1:

global D2
times 0xe1475-($-$$) resb 1
D2:

global D3
times 0xe1479-($-$$) resb 1
D3:

global D4
times 0xe147d-($-$$) resb 1
D4:

global D5
times 0xe1481-($-$$) resb 1
D5:

global D6
times 0xe1485-($-$$) resb 1
D6:

global D7
times 0xe1489-($-$$) resb 1
D7:

global A0
times 0xe148d-($-$$) resb 1
A0:

global A1
times 0xe1491-($-$$) resb 1
A1:

global A2
times 0xe1495-($-$$) resb 1
A2:

global A3
times 0xe1499-($-$$) resb 1
A3:

global A4
times 0xe149d-($-$$) resb 1
A4:

global A5
times 0xe14a1-($-$$) resb 1
A5:

global A6
times 0xe14a5-($-$$) resb 1
A6:

global EGA_graphics
times 0xe14c9-($-$$) resb 1
EGA_graphics:

global setup_dat_buffer
times 0xe14cb-($-$$) resb 1
setup_dat_buffer:

global joyKbdWord
times 0xe14d1-($-$$) resb 1
joyKbdWord:

global videoSpeedIndex
times 0xe14d7-($-$$) resb 1
videoSpeedIndex:

global joy1_X_value
times 0xe14df-($-$$) resb 1
joy1_X_value:

global joy1_center_X
times 0xe14e9-($-$$) resb 1
joy1_center_X:

global joy1_center_Y
times 0xe14eb-($-$$) resb 1
joy1_center_Y:

global joy1_min_Y
times 0xe14f1-($-$$) resb 1
joy1_min_Y:

global joy1_min_X
times 0xe14f3-($-$$) resb 1
joy1_min_X:

global joy1_max_X
times 0xe14f5-($-$$) resb 1
joy1_max_X:

global joy1_max_Y
times 0xe14f7-($-$$) resb 1
joy1_max_Y:

global joy1_X_negative_divisor
times 0xe14f9-($-$$) resb 1
joy1_X_negative_divisor:

global joy1_X_positive_divisor
times 0xe14fb-($-$$) resb 1
joy1_X_positive_divisor:

global joy1_Y_negative_divisor
times 0xe14fd-($-$$) resb 1
joy1_Y_negative_divisor:

global joy1_Y_positive_divisor
times 0xe14ff-($-$$) resb 1
joy1_Y_positive_divisor:

global joy2_X_value
times 0xe1501-($-$$) resb 1
joy2_X_value:

global joy2_center_X
times 0xe150b-($-$$) resb 1
joy2_center_X:

global joy2_center_Y
times 0xe150d-($-$$) resb 1
joy2_center_Y:

global joy2_min_Y
times 0xe1513-($-$$) resb 1
joy2_min_Y:

global joy2_min_X
times 0xe1515-($-$$) resb 1
joy2_min_X:

global joy2_max_X
times 0xe1517-($-$$) resb 1
joy2_max_X:

global joy2_max_Y
times 0xe1519-($-$$) resb 1
joy2_max_Y:

global joy2_X_negative_divisor
times 0xe151b-($-$$) resb 1
joy2_X_negative_divisor:

global joy2_X_positive_divisor
times 0xe151d-($-$$) resb 1
joy2_X_positive_divisor:

global joy2_Y_negative_divisor
times 0xe151f-($-$$) resb 1
joy2_Y_negative_divisor:

global joy2_Y_positive_divisor
times 0xe1521-($-$$) resb 1
joy2_Y_positive_divisor:

global joy1Status
times 0xe1523-($-$$) resb 1
joy1Status:

global joy2Status
times 0xe1525-($-$$) resb 1
joy2Status:

global numLoopsJoy1
times 0xe1527-($-$$) resb 1
numLoopsJoy1:

global numLoopsJoy2
times 0xe1529-($-$$) resb 1
numLoopsJoy2:

global UP
times 0xe152b-($-$$) resb 1
UP:

global joy1_fire_1
times 0xe1530-($-$$) resb 1
joy1_fire_1:

global joy1_fire_2
times 0xe1531-($-$$) resb 1
joy1_fire_2:

global joy2_fire_1
times 0xe1532-($-$$) resb 1
joy2_fire_1:

global joy2_fire_2
times 0xe1533-($-$$) resb 1
joy2_fire_2:

global lin_adr_384k
times 0xee152-($-$$) resb 1
lin_adr_384k:

global key_count
times 0xeea98-($-$$) resb 1
key_count:

global scanCode
times 0xeeaa6-($-$$) resb 1
scanCode:

global controlWord
times 0xeeabe-($-$$) resb 1
controlWord:

global convertedKey
times 0xeead0-($-$$) resb 1
convertedKey:

global prevVideoMode
times 0xeead2-($-$$) resb 1
prevVideoMode:

global playGame
times 0xeeccf-($-$$) resb 1
playGame:

global longFireCounter
times 0xeece9-($-$$) resb 1
longFireCounter:

global final_controls_status
times 0xeeced-($-$$) resb 1
final_controls_status:

global controlsHeldTimer
times 0xeecf1-($-$$) resb 1
controlsHeldTimer:

global fire
times 0xeecf3-($-$$) resb 1
fire:

global short_fire
times 0xeecf5-($-$$) resb 1
short_fire:

global longFireTime
times 0xeecf7-($-$$) resb 1
longFireTime:

global longFireFlag
times 0xeecf9-($-$$) resb 1
longFireFlag:

global currentMenuPtr
times 0xeed02-($-$$) resb 1
currentMenuPtr:

global menuFade
times 0xeed08-($-$$) resb 1
menuFade:

global aDataTeam_nnn
times 0xeed32-($-$$) resb 1
aDataTeam_nnn:

global aCustoms_edt
times 0xeed40-($-$$) resb 1
aCustoms_edt:

global teamFileBuffer
times 0xeed68-($-$$) resb 1
teamFileBuffer:

global player1_clear_flag
times 0xfe792-($-$$) resb 1
player1_clear_flag:

global player2_clear_flag
times 0xfe794-($-$$) resb 1
player2_clear_flag:

global saveFileName
times 0xfe81c-($-$$) resb 1
saveFileName:

global hil_filename
times 0xfe838-($-$$) resb 1
hil_filename:

global exitMenu
times 0xfea88-($-$$) resb 1
exitMenu:

global numSpritesToRender
times 0x10032c-($-$$) resb 1
numSpritesToRender:

global lastKey
times 0x100332-($-$$) resb 1
lastKey:

global frameCount
times 0x100334-($-$$) resb 1
frameCount:

global stoppageTimer
times 0x10033a-($-$$) resb 1
stoppageTimer:

global currentTick
times 0x10033c-($-$$) resb 1
currentTick:

global smallCharsTable
times 0x100352-($-$$) resb 1
smallCharsTable:

global conversion_table_small
times 0x100360-($-$$) resb 1
conversion_table_small:

global bigCharsTable
times 0x100440-($-$$) resb 1
bigCharsTable:

global cameraXFraction
times 0x100536-($-$$) resb 1
cameraXFraction:

global cameraX
times 0x100538-($-$$) resb 1
cameraX:

global cameraYFraction
times 0x10053a-($-$$) resb 1
cameraYFraction:

global cameraY
times 0x10053c-($-$$) resb 1
cameraY:

global pl1LastFired
times 0x10055a-($-$$) resb 1
pl1LastFired:

global pl1_fire
times 0x10055f-($-$$) resb 1
pl1_fire:

global pl1ShortFireCounter
times 0x10056a-($-$$) resb 1
pl1ShortFireCounter:

global pl2LastFired
times 0x100572-($-$$) resb 1
pl2LastFired:

global pl2_fire
times 0x100577-($-$$) resb 1
pl2_fire:

global pl2ShortFireCounter
times 0x100584-($-$$) resb 1
pl2ShortFireCounter:

global deltaColor
times 0x100588-($-$$) resb 1
deltaColor:

global team1Scorer1Sprite
times 0x1018d6-($-$$) resb 1
team1Scorer1Sprite:

global team2Scorer1Sprite
times 0x101c46-($-$$) resb 1
team2Scorer1Sprite:

global ballSprite
times 0x102b50-($-$$) resb 1
ballSprite:

global allSpritesArray
times 0x102ff4-($-$$) resb 1
allSpritesArray:

global sortedSprites
times 0x103130-($-$$) resb 1
sortedSprites:

global color_table_shine
times 0x103c58-($-$$) resb 1
color_table_shine:

global playMatchTeam1Ptr
times 0x104b5c-($-$$) resb 1
playMatchTeam1Ptr:

global playMatchTeam2Ptr
times 0x104b60-($-$$) resb 1
playMatchTeam2Ptr:

global contAbortResult
times 0x104bde-($-$$) resb 1
contAbortResult:

global aSetup_dat
times 0x1051ad-($-$$) resb 1
aSetup_dat:

global convert_keys_table
times 0x1052c8-($-$$) resb 1
convert_keys_table:

global pitch_dat_buffer
times 0x10535e-($-$$) resb 1
pitch_dat_buffer:

global currentMenu
times 0x107a8e-($-$$) resb 1
currentMenu:

global tact_4_4_2
times 0x10cc68-($-$$) resb 1
tact_4_4_2:

global tacticsTable
times 0x10de08-($-$$) resb 1
tacticsTable:

global saveFileTitle
times 0x10f544-($-$$) resb 1
saveFileTitle:

global chooseTacticsTeamPtr
times 0x10f5d8-($-$$) resb 1
chooseTacticsTeamPtr:

global editTacticsCurrentTactics
times 0x10f5e0-($-$$) resb 1
editTacticsCurrentTactics:

global left_team_coach_no
times 0x1208e2-($-$$) resb 1
left_team_coach_no:

global right_team_coach_no
times 0x1208e4-($-$$) resb 1
right_team_coach_no:

global left_team_player_no
times 0x1208e6-($-$$) resb 1
left_team_player_no:

global right_team_player_no
times 0x1208e8-($-$$) resb 1
right_team_player_no:

global isGameFriendly
times 0x1208ec-($-$$) resb 1
isGameFriendly:

global pl1Tactics
times 0x1208fa-($-$$) resb 1
pl1Tactics:

global pl2Tactics
times 0x1208fc-($-$$) resb 1
pl2Tactics:

global leftTeamPtr
times 0x120b01-($-$$) resb 1
leftTeamPtr:

global rightTeamPtr
times 0x120b05-($-$$) resb 1
rightTeamPtr:

global showingStats
times 0x120b35-($-$$) resb 1
showingStats:

global showStats
times 0x120b37-($-$$) resb 1
showStats:

global statsTimeout
times 0x120b39-($-$$) resb 1
statsTimeout:

global team1Scorers
times 0x120b3d-($-$$) resb 1
team1Scorers:

global team2Scorers
times 0x120d4d-($-$$) resb 1
team2Scorers:

global goal_base_ptr
times 0x1227e2-($-$$) resb 1
goal_base_ptr:

global nextGoalPtr
times 0x1227e6-($-$$) resb 1
nextGoalPtr:

global replayState
times 0x1227ea-($-$$) resb 1
replayState:

global current_hil_ptr
times 0x1227f4-($-$$) resb 1
current_hil_ptr:

global goalCounter
times 0x122836-($-$$) resb 1
goalCounter:

global hil_file_buffer
times 0x127274-($-$$) resb 1
hil_file_buffer:

global hil_num_goals
times 0x128094-($-$$) resb 1
hil_num_goals:

global menuCycleTimer
times 0x1566dc-($-$$) resb 1
menuCycleTimer:

global vsPtr
times 0x156758-($-$$) resb 1
vsPtr:

global stateGoal
times 0x1588c2-($-$$) resb 1
stateGoal:

global spritesIndex
times 0x158def-($-$$) resb 1
spritesIndex:

global found_filenames_buffer
times 0x15b3a8-($-$$) resb 1
found_filenames_buffer:

global inputing_text_ok
times 0x15fe50-($-$$) resb 1
inputing_text_ok:

global PIT_countdown
times 0x161450-($-$$) resb 1
PIT_countdown:

global aAsave_256
times 0x161645-($-$$) resb 1
aAsave_256:

global SWOS_StackTop
times 0x1616eb-($-$$) resb 1
SWOS_StackTop:

global inSubstitutesMenu
times 0x161866-($-$$) resb 1
inSubstitutesMenu:

global goToSubsTimer
times 0x161870-($-$$) resb 1
goToSubsTimer:

global bench1Called
times 0x161872-($-$$) resb 1
bench1Called:

global bench2Called
times 0x161874-($-$$) resb 1
bench2Called:

global subsMenuX
times 0x16187c-($-$$) resb 1
subsMenuX:

global formationMenuX
times 0x16187e-($-$$) resb 1
formationMenuX:

global subsState
times 0x16189c-($-$$) resb 1
subsState:

global selectedFormationEntry
times 0x16189e-($-$$) resb 1
selectedFormationEntry:

global plToBeSubstitutedOrd
times 0x1618a0-($-$$) resb 1
plToBeSubstitutedOrd:

global plToBeSubstitutedPos
times 0x1618a2-($-$$) resb 1
plToBeSubstitutedPos:

global plToEnterGameIndex
times 0x1618a4-($-$$) resb 1
plToEnterGameIndex:

global benchTeam
times 0x1618a6-($-$$) resb 1
benchTeam:

global currentSubsTeam
times 0x1618aa-($-$$) resb 1
currentSubsTeam:

global benchCounter
times 0x1618b0-($-$$) resb 1
benchCounter:

global benchCounter2
times 0x1618b2-($-$$) resb 1
benchCounter2:

global selectedPlayerInSubs
times 0x1618ce-($-$$) resb 1
selectedPlayerInSubs:

global seed
times 0x1618d0-($-$$) resb 1
seed:

global paused
times 0x1618d4-($-$$) resb 1
paused:

global replaySelected
times 0x161928-($-$$) resb 1
replaySelected:

global pitch_type_str_table
times 0x1621c6-($-$$) resb 1
pitch_type_str_table:

global game_length_str_table
times 0x1621f0-($-$$) resb 1
game_length_str_table:

global SWOS_MainMenu
times 0x1623c8-($-$$) resb 1
SWOS_MainMenu:

global playMatchMenu
times 0x16571b-($-$$) resb 1
playMatchMenu:

global continueAbortMenu
times 0x16608c-($-$$) resb 1
continueAbortMenu:

global sineCosineTable
times 0x1696bc-($-$$) resb 1
sineCosineTable:

global chosenTactics
times 0x1699fc-($-$$) resb 1
chosenTactics:

global editTacticsMenu
times 0x169d66-($-$$) resb 1
editTacticsMenu:

global animPatternsState
times 0x16d06a-($-$$) resb 1
animPatternsState:

global gameStoppedTimer
times 0x16d728-($-$$) resb 1
gameStoppedTimer:

global alreadyScoredTable
times 0x16d7c2-($-$$) resb 1
alreadyScoredTable:

global firstGoalScoredTable
times 0x16d7ce-($-$$) resb 1
firstGoalScoredTable:

global advert1Pointers
times 0x16d81a-($-$$) resb 1
advert1Pointers:

global advert2Pointers
times 0x16d83a-($-$$) resb 1
advert2Pointers:

global advert3Pointers
times 0x16d85a-($-$$) resb 1
advert3Pointers:

global gameTime
times 0x16da7c-($-$$) resb 1
gameTime:

global teamSwitchCounter
times 0x16dbe0-($-$$) resb 1
teamSwitchCounter:

global team1StatsData
times 0x16dbe2-($-$$) resb 1
team1StatsData:

global leftTeamData
times 0x16dbfe-($-$$) resb 1
leftTeamData:

global rightTeamData
times 0x16dc8f-($-$$) resb 1
rightTeamData:

global gameStatePl
times 0x16dd3e-($-$$) resb 1
gameStatePl:

global gameState
times 0x16dd40-($-$$) resb 1
gameState:

global eventTimer
times 0x16dd44-($-$$) resb 1
eventTimer:

global halfNumber
times 0x16dd5c-($-$$) resb 1
halfNumber:

global teamPlayingUp
times 0x16dd60-($-$$) resb 1
teamPlayingUp:

global _STACKLOW
times 0x1732fc-($-$$) resb 1
_STACKLOW:

global _STACKTOP
times 0x173300-($-$$) resb 1
_STACKTOP:

