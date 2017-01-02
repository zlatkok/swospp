; Multiplayer options menu, actual menu stream.
;

[list -]
%include "swos.inc"
%include "swsmenu.inc"
[list +]

section .data

    ;
    ; multiplayer options menu
    ;

    extern InitializeMPOptionsMenu, MPOptionsMenuAfterDraw, IncreaseNetworkTimeout, DecreaseNetworkTimeout
    extern IncreaseSkipFrames, DecreaseSkipFrames
    extern ChooseMPTactics, ExitMultiplayerOptions
    extern NetworkTimeoutBeforeDraw, SkipFramesBeforeDraw, mpOptSelectTeamBeforeDraw
    extern MP_Tactics, MPMenuSetPlayerNick, ChooseTeam

    StartMenu mpOptionsMenu, InitializeMPOptionsMenu, MPOptionsMenuAfterDraw, 0, 1
        %assign START_Y                 30
        %assign WIDTH_COLUMN_1          124
        %assign WIDTH_COLUMN_2          136
        %assign OPTION_HEIGHT           11
        %assign X_COLUMN_1              (WIDTH - WIDTH_COLUMN_1 - WIDTH_COLUMN_2 - 5) / 2
        %assign X_COLUMN_2              X_COLUMN_1 + WIDTH_COLUMN_1 + 5
        %assign CHANGER_WIDTH           16
        %assign CHANGER_HEIGHT          11
        %assign CHANGER_OPTION_WIDTH    WIDTH_COLUMN_2 - CHANGER_WIDTH * 2 - 2

        MenuXY -8, 0
        ; [0] "nickname"
        StartEntry X_COLUMN_1, START_Y, WIDTH_COLUMN_1, OPTION_HEIGHT
            EntryColor 9
            EntryString 0, "NICKNAME"
        EndEntry

        ; [1] nickname value
        StartEntry X_COLUMN_2, previousEntryY, WIDTH_COLUMN_2, OPTION_HEIGHT, nickname
            NextEntries -1, -1, -1, 3
            EntryColor 13
            EntryString 0, -1
            OnSelect MPMenuSetPlayerNick
        EndEntry

        ; [2] "network timeout"
        StartEntry X_COLUMN_1, previousEntryY + 15, WIDTH_COLUMN_1, OPTION_HEIGHT
            EntryColor 9
            EntryString 0, "NETWORK TIMEOUT"
        EndEntry

        ; [3] "+" to increase network timeout
        StartEntry X_COLUMN_2, previousEntryY, CHANGER_WIDTH, CHANGER_HEIGHT, incNetworkTimeout
            NextEntries -1, 5, mpOptionsMenu_nickname, 7
            EntryColor 13
            EntryString 0, aPlus
            OnSelect IncreaseNetworkTimeout
        EndEntry

        ; [4] network timeout value
        StartEntry previousEntryEndX + 1, previousEntryY, CHANGER_OPTION_WIDTH, OPTION_HEIGHT
            EntryColor 13
            EntryString 0, -1
            BeforeDraw NetworkTimeoutBeforeDraw
        EndEntry

        ; [5] "-" to decrease network timeout
        StartEntry previousEntryEndX + 1, previousEntryY, CHANGER_WIDTH, CHANGER_HEIGHT, decNetworkTimeout
            NextEntries mpOptionsMenu_incNetworkTimeout, -1, mpOptionsMenu_nickname, 9
            EntryColor 13
            EntryString 0, aMinus
            OnSelect DecreaseNetworkTimeout
        EndEntry

        ; [6] "skip frames"
        StartEntry X_COLUMN_1, previousEntryY + 15, WIDTH_COLUMN_1, OPTION_HEIGHT
            EntryColor 9
            EntryString 0, "SKIP FRAMES"
        EndEntry

        ; [7] "+" to increase skip frames
        StartEntry X_COLUMN_2, previousEntryY, CHANGER_WIDTH, CHANGER_HEIGHT, incSkipFrames
            NextEntries -1, 9, mpOptionsMenu_incNetworkTimeout, 11
            EntryColor 13
            EntryString 0, aPlus
            OnSelect IncreaseSkipFrames
        EndEntry

        ; [8] skip frames value
        StartEntry previousEntryEndX + 1, previousEntryY, CHANGER_OPTION_WIDTH, OPTION_HEIGHT
            EntryColor 13
            EntryString 0, -1
            BeforeDraw SkipFramesBeforeDraw
        EndEntry

        ; [9] "-" to decrease skip frames
        StartEntry previousEntryEndX + 1, previousEntryY, CHANGER_WIDTH, CHANGER_HEIGHT, decSkipFrames
            NextEntries mpOptionsMenu_incSkipFrames, -1, mpOptionsMenu_decNetworkTimeout, 11
            EntryColor 13
            EntryString 0, aMinus
            OnSelect DecreaseSkipFrames
        EndEntry

        ; [10] "team"
        StartEntry X_COLUMN_1, previousEntryY + 15, WIDTH_COLUMN_1, OPTION_HEIGHT
            EntryColor 9
            EntryString 0, "TEAM"
        EndEntry

        ; [11] selected team
        StartEntry X_COLUMN_2, previousEntryY, WIDTH_COLUMN_2, OPTION_HEIGHT, selectedTeam
            NextEntries -1, -1, mpOptionsMenu_decSkipFrames, 16
            EntryColor  12
            EntryString 0, 0
            OnSelect    ChooseTeam
            BeforeDraw  mpOptSelectTeamBeforeDraw
        EndEntry

        ; [12] tactics title
        StartEntry (WIDTH - WIDTH_COLUMN_1) / 2, previousEntryY + 20, WIDTH_COLUMN_1, OPTION_HEIGHT
            EntryColor 9
            EntryString 0, "EDIT TACTICS:"
        EndEntry

        ; [13-18] custom tactics, 2 columns, 3 rows
        MarkNextEntryOrdinal    mpOptionsTacticsStart
        %assign TACTICS_WIDTH   140
        %assign TACTICS_HEIGHT  15
        %assign TACTICS_X       (WIDTH - 2 * TACTICS_WIDTH - 5) / 2
        %assign TACTICS_Y       previousEntryY + 20

        StartTemplateEntry
        StartEntry
            EntryColor  0x0e
            OnSelect ChooseMPTactics
        EndEntry

        StartEntry  TACTICS_X, TACTICS_Y, TACTICS_WIDTH, TACTICS_HEIGHT
            NextEntries -1, mpOptionsTacticsStart + 3, mpOptionsMenu_selectedTeam, currentEntry + 1
            EntryString 0, MP_Tactics
        EndEntry
        StartEntry  previousEntryX, previousEntryEndY + 4, TACTICS_WIDTH, TACTICS_HEIGHT
            NextEntries -1, mpOptionsTacticsStart + 4, currentEntry - 1, currentEntry + 1
            EntryString 0, MP_Tactics + TACTICS_SIZE
        EndEntry
        StartEntry  previousEntryX, previousEntryEndY + 4, TACTICS_WIDTH, TACTICS_HEIGHT
            NextEntries -1, mpOptionsTacticsStart + 5, currentEntry - 1, 19
            EntryString 0, MP_Tactics + 2 * TACTICS_SIZE
        EndEntry
        StartEntry  previousEntryEndX + 5, TACTICS_Y, TACTICS_WIDTH, TACTICS_HEIGHT
            NextEntries mpOptionsTacticsStart, -1, mpOptionsMenu_selectedTeam, currentEntry + 1
            EntryString 0, MP_Tactics + 3 * TACTICS_SIZE
        EndEntry
        StartEntry  previousEntryX, previousEntryEndY + 4, TACTICS_WIDTH, TACTICS_HEIGHT
            NextEntries mpOptionsTacticsStart + 1, -1, currentEntry - 1, currentEntry + 1
            EntryString 0, MP_Tactics + 4 * TACTICS_SIZE
        EndEntry
        StartEntry  previousEntryX, previousEntryEndY + 4, TACTICS_WIDTH, TACTICS_HEIGHT
            NextEntries mpOptionsTacticsStart + 2, -1, currentEntry - 1, 19
            EntryString 0, MP_Tactics + 5 * TACTICS_SIZE
        EndEntry

        ; [19] exit
        StartEntry 110, 185, 100, 15
            NextEntries -1, -1, mpOptionsTacticsStart + 5, -1
            EntryColor  12
            EntryString 0, aExit
            OnSelect    ExitMultiplayerOptions
        EndEntry

        ; [20] title
        StartEntry  87, 0, 130, 15
            EntryColor  0x17
            EntryString 0, "MULTIPLAYER OPTIONS"
        EndEntry

    EndMenu


    ;
    ; direct connect menu (at startup)
    ;

    extern SearchForTheGameInit, UpdateGameSearch

    StartMenu directConnectMenu, SearchForTheGameInit, 0, UpdateGameSearch, 1

        StartEntry 2, 40, 300, 40
            EntryColor 0x0c
        EndEntry

        StartEntry 89, 60, 0, 0
            EntryString 0x8010, "SEARCHING FOR GAME..."
        EndEntry

    EndMenu


section .text

global ShowMPOptions
ShowMPOptions:
        mov  [A6], dword mpOptionsMenu
        jmpa ShowMenu


; GetDirectConnectMenu
;
; Return direct connect menu to go directly into at startup if requested.
;
global GetDirectConnectMenu
GetDirectConnectMenu:
        mov  eax, directConnectMenu
        retn
