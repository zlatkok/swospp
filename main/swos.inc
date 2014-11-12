; Automatically generated from swos.map - do not edit

; screen width and height
%define WIDTH       320
%define HEIGHT      200

; screen width during the game
%define GAME_WIDTH  384

; size of tactics structure
%define TACTICS_SIZE 370

; Declare and put string into destination.
; %1 - destination, %2 - string
;
%macro movstr 2.nolist
    [section .strdata]
    %%str:
        db %2, 0
    __SECT__
    mov  %1, %%str
%endmacro

; Push offset of a string, or declare it first if needed
; %1 - string (offset or string literal)
;
%macro pushstr 1.nolist
    %ifstr %1
        [section .strdata]
        %%str:
            db %1, 0
        __SECT__
        push %%str
    %else
        push %1
    %endif
%endmacro

; Declare global variable with specified size.
; %1 - variable name, %2 - variable size
;
%macro declareGlobal 2.nolist
    global %1
    [section .bss]
    %1:
        resb %2
    __SECT__
%endmacro

; Declare static variable with specified size.
; %1 - variable name, %2 - variable size
;
%macro declareStatic 2.nolist
    [section .bss]
    %1:
        resb %2
    __SECT__
%endmacro

; Declare pointer to string, and string itself
%macro declareStr 1.nolist
%ifstr %1
    [section .strdata]
    %%str:
        db %1, 0
    __SECT__
    dd %%str
%else
    %error "Expecting string parameter"
%endif
%endmacro

extern swosCodeBase
extern swosDataBase
extern SWOS
extern GameLoop
extern main_loop
extern Rand
extern MainKeysCheck
extern EnqueueSubstituteSample
extern Initialization
extern TimerProc
extern Int9KeyboardHandler
extern SetPrevVideoModeEndProgram
extern SWOS_EndProgram
extern ProgramPIT
extern WaitRetrace
extern VerifyJoypadControls
extern ReadGamePort
extern Joy1SetStatus
extern Joy2SetStatus
extern Player1StatusProc
extern Player2StatusProc
extern WriteFile
extern FlipInMenu
extern SetControlWord
extern GetKey
extern DumpTimerVariables
extern RestoreOldInt9Handler
extern DumpScreen
extern CDROM_StopAudio
extern SWOS_Text2Sprite
extern CopySprite
extern DrawSpriteCentered
extern SWOS_DrawSprite
extern ClearSprite
extern SWOS_DrawSpriteInGame
extern DrawSpriteInColor
extern DarkenRectangle
extern SWOS_DrawSprite16Pixels
extern SaveSprite
extern ClearBackground
extern ShowMenu
extern CallAfterDraw
extern SetCurrentEntry
extern SetExitMenuFlag
extern ReadTimerDelta
extern PrepareMenu
extern CalcMenuEntryAddress
extern DrawMenu
extern MenuProc
extern DrawMenuItem
extern DrawMenuFrame
extern DrawMenuBack
extern DrawMenuText
extern DrawMenuTextCentered
extern GetTextSize
extern PrimitivePrintf
extern Int2Ascii
extern DrawOuterFrame
extern CheckControls
extern InputText
extern FilterKeys
extern InitializeTacticsPositions
extern ChangePitchType
extern ChangeGameLength
extern SetDefaultOptions
extern SaveOptions
extern RestoreOptions
extern SWOSMainMenuInit
extern CommonMenuExit
extern StartContest
extern SaveCareerSelected
extern InitAndPlayGame
extern SetupPlayers
extern CheckContinueAbortPrompt
extern LoadTeamFile
extern LoadDIYFile
extern SetName
extern PlayMatchAfterDraw
extern ExitPlayMatch
extern PlayMatchSelected
extern ShowErrorMenu
extern DoContinueAbortMenu
extern ExitChooseTeams
extern ChooseTeamsDialog
extern DrawLittlePlayersAndBall
extern InitLittlePlayerSprites
extern SetBenchPlayersNumbers
extern DrawAnimatedPatterns
extern ScrollToCurrent
extern AllowStatistics
extern StatisticsOff
extern UpdateStatistics
extern DrawStatistics
extern BenchCheckControls
extern ShowFormationMenu
extern SubstitutePlayerIfFiring
extern ChangeFormationIfFiring
extern DrawBenchAndSubsMenu
extern DrawBenchPlayersAndCoach
extern DrawSubstitutesMenu
extern DrawSubstitutesMenuEntry
extern DrawFormationMenu
extern SaveCoordinatesForHighlights
extern IncrementHilBufferPointer
extern ValidateHilPointerAdd
extern SetHilFileHeader
extern DrawControlledPlayerNumbers
extern BookPlayer
extern DrawSprites
extern InitTeamsData
extern TeamsControlsCheck
extern CalculateIfPlayerWinsBall
extern GetFilenameAndExtension
extern FindFiles
extern SelFilesBeforeDrawCommon
extern SelectFileToSaveDialog
extern ShowHighlights
extern Flip
extern StopAIL
extern AIL_stop_play
extern swos_start
extern swos_libc_strcmp_
extern swos_libc_memset_
extern swos_libc_time_
extern swos_libc_ctime_
extern swos_libc_strlen_
extern swos_libc_strnicmp_
extern swos_libc_memmove_
extern swos_libc_strncpy_
extern swos_libc_segread_
extern swos_libc_int386x_
extern swos_libc_stackavail_
extern swos_libc_exit_
extern swos_libc_strncmp_
extern aPlayer_0
extern aPlayerCoach_0
extern aCoach_3
extern aHigh
extern aUSER_A
extern aUSER_B
extern aUSER_C
extern aUSER_D
extern aUSER_E
extern aUSER_F
extern aReplay
extern aComputer
extern aContinue
extern aOn
extern aOff
extern aOk
extern aExit
extern aAbort
extern aPlus
extern aMinus
extern aSubstitutes
extern aFrom
extern aPitchType
extern aGameLength
extern currentTeamNumber
extern gameType
extern USER_A
extern USER_B
extern USER_C
extern USER_D
extern USER_E
extern USER_F
extern gameLength
extern autoReplays
extern menuMusic
extern autoSaveHighlights
extern allPlayerTeamsEqual
extern pitchType
extern commentary
extern chairmanScenes
extern seed2
extern random_seed
extern selTeamsPtr
extern numSelectedTeams
extern selectedTeamsBuffer
extern D0
extern D1
extern D2
extern D3
extern D4
extern D5
extern D6
extern D7
extern A0
extern A1
extern A2
extern A3
extern A4
extern A5
extern A6
extern EGA_graphics
extern setup_dat_buffer
extern joyKbdWord
extern videoSpeedIndex
extern joy1_X_value
extern joy1_center_X
extern joy1_center_Y
extern joy1_min_Y
extern joy1_min_X
extern joy1_max_X
extern joy1_max_Y
extern joy1_X_negative_divisor
extern joy1_X_positive_divisor
extern joy1_Y_negative_divisor
extern joy1_Y_positive_divisor
extern joy2_X_value
extern joy2_center_X
extern joy2_center_Y
extern joy2_min_Y
extern joy2_min_X
extern joy2_max_X
extern joy2_max_Y
extern joy2_X_negative_divisor
extern joy2_X_positive_divisor
extern joy2_Y_negative_divisor
extern joy2_Y_positive_divisor
extern joy1Status
extern joy2Status
extern numLoopsJoy1
extern numLoopsJoy2
extern UP
extern joy1_fire_1
extern joy1_fire_2
extern joy2_fire_1
extern joy2_fire_2
extern lin_adr_384k
extern key_count
extern scanCode
extern controlWord
extern convertedKey
extern prevVideoMode
extern playGame
extern longFireCounter
extern final_controls_status
extern controlsHeldTimer
extern fire
extern short_fire
extern longFireTime
extern longFireFlag
extern currentMenuPtr
extern menuFade
extern aDataTeam_nnn
extern aCustoms_edt
extern teamFileBuffer
extern player1ClearFlag
extern player2ClearFlag
extern saveFileName
extern hil_filename
extern exitMenu
extern numSpritesToRender
extern lastKey
extern frameCount
extern stoppageTimer
extern currentTick
extern smallCharsTable
extern conversion_table_small
extern bigCharsTable
extern cameraXFraction
extern cameraX
extern cameraYFraction
extern cameraY
extern pl1LastFired
extern pl1_fire
extern pl1ShortFireCounter
extern pl2LastFired
extern pl2_fire
extern pl2ShortFireCounter
extern deltaColor
extern team1Scorer1Sprite
extern team2Scorer1Sprite
extern ballSprite
extern allSpritesArray
extern sortedSprites
extern tacticsStringTable
extern color_table_shine
extern playMatchTeam1Ptr
extern playMatchTeam2Ptr
extern contAbortResult
extern aSetup_dat
extern convert_keys_table
extern pitch_dat_buffer
extern currentMenu
extern tact_4_4_2
extern tacticsTable
extern saveFileTitle
extern chooseTacticsTeamPtr
extern editTacticsCurrentTactics
extern left_team_coach_no
extern right_team_coach_no
extern left_team_player_no
extern right_team_player_no
extern isGameFriendly
extern pl1Tactics
extern pl2Tactics
extern leftTeamPtr
extern rightTeamPtr
extern showingStats
extern showStats
extern statsTimeout
extern team1Scorers
extern team2Scorers
extern goal_base_ptr
extern nextGoalPtr
extern replayState
extern current_hil_ptr
extern goalCounter
extern hil_file_buffer
extern hil_num_goals
extern menuCycleTimer
extern vsPtr
extern stateGoal
extern spritesIndex
extern found_filenames_buffer
extern inputing_text_ok
extern PIT_countdown
extern aAsave_256
extern SWOS_StackTop
extern inSubstitutesMenu
extern goToSubsTimer
extern bench1Called
extern bench2Called
extern subsMenuX
extern formationMenuX
extern subsState
extern selectedFormationEntry
extern plToBeSubstitutedOrd
extern plToBeSubstitutedPos
extern plToEnterGameIndex
extern benchTeam
extern currentSubsTeam
extern benchCounter
extern benchCounter2
extern selectedPlayerInSubs
extern seed
extern paused
extern replaySelected
extern pitch_type_str_table
extern game_length_str_table
extern SWOS_MainMenu
extern playMatchMenu
extern continueAbortMenu
extern sineCosineTable
extern chosenTactics
extern editTacticsMenu
extern animPatternsState
extern gameStoppedTimer
extern alreadyScoredTable
extern firstGoalScoredTable
extern advert1Pointers
extern advert2Pointers
extern advert3Pointers
extern gameTime
extern teamSwitchCounter
extern team1StatsData
extern leftTeamData
extern rightTeamData
extern gameStatePl
extern gameState
extern eventTimer
extern halfNumber
extern teamPlayingUp
extern _STACKLOW
extern _STACKTOP

; jumps and calls (opcodes 0xe8 and 0xe9) are relative, so we can't use them
; when calling SWOS code from SWOSPP, and vice versa - absolute calls must be made
;
%macro calla 1-2.nolist
%if %0 == 1
    mov  eax, %1
    call eax
%else
    %ifidn %2, push
        push eax
        mov  eax, %1
        call eax
        pop  eax
    %else
        mov  %2, %1
        call %2
    %endif
%endif
%endmacro

%macro jmpa 1.nolist
    push dword %1
    retn
%endmacro

; callCdecl
;
; %1   - function to call
; %... - parameters to function
;
; Macro for invoking cdecl call convention routines.
;
%macro callCdecl 1-*
    %rep %0 - 1
        %rotate -1              ; rotate right in order to push parameters in reverse order (minus function name)
        pushstr %1              ; string parameters will be detected and automatically declared
    %endrep
    %rotate -1                  ; restore parameter order so we can call the function
    call %1
    add  esp, (%0 - 1) * 4      ; cdecl must do stack cleanup
%endmacro

; AsmWriteToLogFunc
;
; %1... - printf-like parameters
;
; Macro for using log file support - use it just like in C
;
%macro AsmWriteToLogFunc 1+
    pushad                      ; we must not interfere with normal code execution
    pushf
    callCdecl _WriteToLogFunc, %{1}     ; gotta escape param here... or else
    popf
    popad
%endmacro

%ifdef DEBUG
extern _WriteToLogFunc
%macro WriteToLog 1+
    AsmWriteToLogFunc %1
%endmacro
%else
%macro WriteToLog 1+
    ; nothing
%endmacro
%endif

