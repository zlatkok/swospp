; Contains multiplayer menu, and all menus derived from it

[list -]
%include "swos.inc"
%include "swsmenu.inc"
%include "mplayer.inc"
[list +]

extern GetPreviousMenu, ReturnToMenu, SetMenuToReturnToAfterTheGame
extern GetSkipFrames, SetSkipFrames
extern GetNumSubstitutes, SetNumSubstitutes, GetMaxSubstitutes, SetMaxSubstitutes
extern strupr, GetDirectGameName

bits 32
section .data

    ;
    ; multiplayer menu
    ;

    extern GetPlayerNick, SetPlayerNick, GetGameName, SetGameName, SetTeam, ShowMPOptions
    global multiplayerMenu

    StartMenu multiplayerMenu, InitMultiplayerMenu, MPMenuAfterDraw, 0, 4

        ; [0] "nickname"
        StartEntry 53, 45, 60, 11
            EntryColor 9
            EntryString 0, "NICKNAME"
        EndEntry

        ; [1] enter name
        StartEntry 115, 45, 136, 11, playerNick
            NextEntries -1, -1, -1, 3
            EntryColor 13
            EntryString 0, -1
            OnSelect MPMenuSetPlayerNick
        EndEntry

        ; [2] "game name"
        StartEntry 53, 60, 60, 11
            EntryColor 9
            EntryString 0, "GAME NAME"
        EndEntry

        ; [3] enter game name
        StartEntry 115, 60, 136, 11, gameName
            NextEntries -1, -1, 1, 4
            EntryColor 13
            EntryString 0, -1
            OnSelect MPMenuSetGameName
        EndEntry

        ; [4] create game
        StartEntry 92, 80, 120, 15
            NextEntries -1, -1, 3, 5
            EntryColor 14
            EntryString 0, "CREATE GAME"
            OnSelect    ShowGameLobbyMenu
        EndEntry

        ; [5] join game
        StartEntry 92, 105, 120, 15
            NextEntries -1, -1, 4, 6
            EntryColor 14
            EntryString 0, aJoinGame
            OnSelect    ShowJoinGameMenu
        EndEntry

        ; [6] options
        StartEntry 92, 130, 120, 15
            NextEntries -1, -1, 5, 7
            EntryColor 14
            EntryString 0, "OPTIONS"
            OnSelect    ShowMPOptions
        EndEntry

        ; [7] exit
        StartEntry 102, 185, 100, 15
            NextEntries -1, -1, 6, -1
            EntryColor  12
            EntryString 0, aExit
            OnSelect    ExitMultiplayerMenu
        EndEntry

        ; [8] title
        StartEntry  92, 0, 120, 15
            EntryColor  0x17
            EntryString 0, "MULTIPLAYER"
        EndEntry

    EndMenu

section .text

; functions from Multiplayer menu

extern ShutDownNetwork, InitPlayerNick, InitGameName,
extern InitMultiplayer, FinishMultiplayer

InitMultiplayerMenu:
        call InitMultiplayer        ; forbid entering this menu if multiplayer isn't working for some reason
        test eax, eax
        jnz  .err_exit

        call PatchMenu
        jmp  MPMenuAfterDraw

.err_exit:
        call strupr                 ; it'll tell us what the error was
        mov dword [A0], eax

ShowErrorMenuAndExit:
        calla ShowErrorMenu
        jmpa SetExitMenuFlag


; SetEntryString
;
; in:
;     eax -> source string that will be put to menu entry
;     A5  -> menu ordinal of which to set string
;
; Set specific menu entry text to given string.
;
extern strcpy
SetEntryString:
        mov  ecx, eax
        calla CalcMenuEntryAddress
        mov  edx, ecx
        mov  esi, [A0]
        mov  eax, [esi + MenuEntry.string]
        jmpa strcpy


; Fill game name and player nickname and prevent them appearing as "STDMENUTEXT".
MPMenuAfterDraw:
        push byte multiplayerMenu_playerNick
        call GetPlayerNick
        pop  dword [D0]
        call SetEntryString

        push byte multiplayerMenu_gameName
        call GetGameName
        pop  dword [D0]
        jmp  SetEntryString


ExitMultiplayerMenu:
        call UnpatchMenu
        call FinishMultiplayer
        jmpa SetExitMenuFlag


; InputStringToEntryAndSave
;
; in:
;     A5  -> menu entry whose string buffer will be used for input
;     ebx -  buffer max length (limit for string input)
;     ecx -> store function, expects pointer to string that received the input in eax
;
InputStringToEntryAndSave:
        mov  esi, [A5]
        mov  eax, [esi + MenuEntry.string]
        mov  [A0], eax
        mov  dword [D0], ebx
        push eax        ; save string buffer and pointer to function
        push ecx
        calla InputText
        pop  ecx
        pop  eax
        jnz  .out

        call ecx

.out:
        retn


MPMenuSetGameName:
        push byte GAME_NAME_LENGTH
        mov  ecx, SetGameName
        pop  ebx
        jmp  InputStringToEntryAndSave


global MPMenuSetPlayerNick
MPMenuSetPlayerNick:
        push byte NICKNAME_LEN
        mov  ecx, SetPlayerNick
        pop  ebx
        jmp  InputStringToEntryAndSave


ShowGameLobbyMenu:
        call GetPlayerNick
        mov  al, [eax]          ; don't let them in without a nickname
        test al, al
        jz   .nick_error

        call GetGameName
        mov  al, [eax]          ; don't let them in without game name either
        test al, al
        jz   .game_name_error

        mov  byte [weAreTheServer], 1
        mov  dword [A6], gameLobbyMenu
        jmpa ShowMenu

.game_name_error:
        movstr dword [A0], "PLEASE ENTER GAME NAME"
        jmp  short .out

.nick_error:
        movstr dword [A0], "PLEASE ENTER NICKNAME"

.out:
        jmpa ShowErrorMenu


; ReturnToMPMenuAfterTheGame [called from C++]
;
; Make sure we are returning to multiplayer menu, in case choose teams menu was open.
;
ReturnToMPMenuAfterTheGame:
        pushad
        xor  ebx, ebx
        xor  eax, eax
        mov  al, [weAreTheServer]
        cmp  ebx, eax
        sbb  eax, eax
        not  eax
        neg  eax
        lea  edx, [eax + 4]
        mov  eax, multiplayerMenu
        call SetMenuToReturnToAfterTheGame
        mov  byte [choosingTeam], 0
        calla ExitChooseTeams
        calla SetExitMenuFlag
        popad
        retn


UnpatchSWOSProc:
        mov  byte [SWOS + 0x24b], 0
        jmpa RestorePreviousMenu


; ReturnToGameLobbyAfterTheGame [called from C++]
;
ReturnToGameLobbyAfterTheGame:
        pushad
        mov  eax, gameLobbyMenu
        cmp  [currentMenuPtr], eax
        jz   .out

        mov  edx, [gameLobbySelectedEntry]
        call SetMenuToReturnToAfterTheGame
        mov  byte [choosingTeam], 0
        calla ExitChooseTeams           ; will exit menu, and luckily we know it can be _max_ one menu :P
        call UnpatchAfterSettingTeams
        mov  eax, [currentMenuPtr]
        cmp  eax, continueAbortMenu
        jz   .out

        calla ExitPlayMatch
        mov  eax, gameLobbyMenu
        xor  edx, edx
        call ReturnToMenu
        mov  byte [SWOS + 0x24b], 1     ; SWOS disallows returning back from menu that called game
        lea  eax, [UnpatchSWOSProc - 5] ; choose teams is toughie
        lea  ebx, [SWOS + 0x259]
        sub  eax, ebx
        mov  dword [SWOS + 0x25a], eax

.out:
        popad
        retn


; SetGameLobbyMenuServerMode
;
; in:
;     al - boolean, server mode true or false
;
; Control behaviour of game lobby menu, are we showing it in server or client mode.
;
global SetGameLobbyMenuServerMode
SetGameLobbyMenuServerMode:
        mov  [weAreTheServer], al
        retn


section .data

    ;
    ; join game menu
    ;

    StartMenu joinGameMenu, InitJoinGameMenu, 0, 0, 13
        ; [0] menu title
        StartEntry 92, 0, 120, 15
            EntryColor  0x17
            EntryString 0, aJoinGame
        EndEntry

        ; [1] game list title
        %assign x 92
        %assign width 120
        StartEntry  x, 20, width, 15
            EntryColor 9
            EntryString 0, "CURRENT GAMES:"
        EndEntry

        ; games template entry
        StartTemplateEntry
        StartEntry
            EntryColor 13
            EntryString 0, 0
            InvisibleEntry
            OnSelect OnJoinGame
        EndEntry

        MarkNextEntryOrdinal mpGamesStartIndex
        ; [2-11] found games 10 entries
        %assign y 41
        %assign prev -1
        %assign index 2
        %rep 10
            %assign next index + 1
            StartEntry x - width / 2, y, 2 * width, 11
            NextEntries -1, -1, prev, next
            EndEntry
            %assign y y + 12
            %assign prev index
            %assign index index + 1
        %endrep
        RestoreTemplateEntry
        MarkNextEntryOrdinal mpGamesEndIndex

        ; [12] refresh
        StartEntry x, 200 - 2 * 15 - 4, width, 15, Refresh
            EntryString 0, aRefresh
            EntryColor 14
            OnSelect RequestListRefresh
            NextEntries -1, -1, prev, index + 1
        EndEntry

        ; [13] exit
        StartEntry 102, 200 - 15, 100, 15, Exit
            EntryString 0, aExit
            EntryColor 4
            OnSelect ExitJoinGameMenu
            NextEntries -1, -1, prev + 1, -1
        EndEntry
    EndMenu

aRefresh:
    db "REFRESH", 0
aSearching:
    db "SEARCHING...", 0


section .text


ShowJoinGameMenu:
        mov dword [A6], joinGameMenu
        jmpa ShowMenu


extern EnterWaitingToJoinState
InitJoinGameMenu:
        mov  eax, RefreshWaitingGames
        xor  edx, edx
        call EnterWaitingToJoinState
        retn


extern LeaveWaitingToJoinState
ExitJoinGameMenu:
        call LeaveWaitingToJoinState
        jmpa SetExitMenuFlag


; Refresh button is pressed.
extern RefreshList
RequestListRefresh:
        call RefreshList
        mov  word [prevRefreshDisabled], 0
        retn


; ToggleRefreshing
;
; in:
;     eax - true if currently refreshing
;
; Toggle refresh button between "REFRESH" and "SEARCHING" labels. While it's
; refreshing disable it, and switch to exit button.
;
ToggleRefreshing:
        push eax
        mov  [D0], word joinGameMenu_Refresh
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  [esi + MenuEntry.string], dword aRefresh   ; assume not searching
        mov  [esi + MenuEntry.backAndFrameColor], word 14
        pop  eax
        mov  [esi + MenuEntry.isDisabled], ax
        test eax, eax
        jz   .out

        mov  [esi + MenuEntry.string], dword aSearching
        mov  [esi + MenuEntry.backAndFrameColor], word 9
        mov  eax, [g_currentMenu]

        ; check if there are any games listed
        xor  ecx, ecx
        xor  ebx, ebx
        mov  cx, mpGamesStartIndex

.nextEntry:
        mov  [D0], cx
        push ebx
        calla CalcMenuEntryAddress
        pop  ebx
        mov  esi, [A0]
        cmp  word [esi + MenuEntry.isInvisible], 0
        jnz  .continue

        mov  ebx, ecx

.continue:
        inc  ecx
        add  esi, MenuEntry_size
        cmp  ecx, joinGameMenu_Refresh
        jb   .nextEntry

        test ebx, ebx
        jz   .out

        cmp  word [prevRefreshDisabled], 0
        jnz  .out

        mov  [D0], word joinGameMenu_Exit
        calla SetCurrentEntry
        add  word [prevRefreshDisabled], 1
.out:
        retn


; EnterGameLobby [called from C++]
;
; We have been granted the privilege by the gaming gods.
;
EnterGameLobby:
        pushad
        mov  byte [weAreTheServer], 0
        mov  dword [A6], gameLobbyMenu
        calla ShowMenu
        popad
        retn


; RefreshWaitingGames [called from C++]
;
; in:
;      eax -> pointer to state info structure, see below for details
;
; Refresh graphical state of the waiting to join games menu. We get called from
; network loop, which sends us complete state in an array with the following
; structure:
;   0 dword, boolean: true if we're still refreshing
;   4 dword, pointer to string: name of n-th game (note: there may be 0 games!)
;      ...     ...
;   x dword, value -1: terminator for the string array
;
RefreshWaitingGames:
        pushad
        push eax
        add  eax, 4
        mov  [D0], dword mpGamesStartIndex
        push eax
        calla CalcMenuEntryAddress
        pop  eax
        mov  esi, [A0]      ; esi -> current entry
        mov  [D0], dword mpGamesEndIndex
        push eax
        calla CalcMenuEntryAddress
        pop  eax
        mov  edi, [A0]      ; edi -> sentry entry

.next_ptr:
        cmp  [eax], dword -1
        jz   .clear_games_loop

        mov  [esi + MenuEntry.isInvisible], word 0
        mov  ebx, [eax]
        mov  [esi + MenuEntry.string], ebx
        add  eax, 4
        add  esi, MenuEntry_size
        jmp  .next_ptr

.clear_games_loop:          ; make any remaining entries invisible
        cmp  esi, edi
        jge  .toggle_refresh
        mov  [esi + MenuEntry.isInvisible], word 1
        add  esi, MenuEntry_size
        jmp  .clear_games_loop

.toggle_refresh:
        pop  eax
        mov  eax, [eax]
        call ToggleRefreshing
        popad
        retn


; JoinRemoteGame
;
; in:
;     eax -  joining game ordinal
;
; Display modal "connecting" dialog while trying to connect to the selected game.
;
extern JoinGame, InitModalDialog
global JoinRemoteGame
JoinRemoteGame:
        push eax
        mov  eax, modalDialogInitData
        mov  dword [modalDialogStrTable], connectingPleaseWaitStrTable
        call InitModalDialog
        pop  eax
        mov  edx, JoinGameModalDialogProc
        mov  ecx, EnterGameLobby
        push PlayMatchMenuTimeout
        push SyncModalDialogProc
        push ReturnToGameLobbyAfterTheGame
        push ReturnToMPMenuAfterTheGame
        push ShowPlayMatchMenu
        push SyncModalDialogProc
        push DisconnectedFromLobby
        call JoinGame           ; has stack frame, do not "optimize"
        retn


OnJoinGame:
        mov  esi, [A5]
        mov  eax, [esi + MenuEntry.ordinal]
        sub  eax, mpGamesStartIndex
        call JoinRemoteGame
        mov  word [D0], joinGameMenu_Exit
        jmpa SetCurrentEntry


; ExitSWOS
;
; Switch back to previous screen resolution and close SWOS gracefully.
;
extern EndProgram, SwitchToPrevVideoMode
ExitSWOS:
        call SwitchToPrevVideoMode
        xor  eax, eax
        jmp  EndProgram


; ExitIfInDirectMode
;
; Exit SWOS if we are in direct connect mode.
;
ExitIfInDirectMode:
        call GetDirectGameName
        mov  al, [eax]
        test al, al
        jz   .out

        call DisbandGame
        jmp  ExitSWOS

.out:
        retn


; DisconnectedFromLobby [called from C++]
;
; Called when we get disconnected from the lobby. Show error message and go
; back to trying to find some games, unless in direct connect mode -- in that
; case exit SWOS.
;
DisconnectedFromLobby:
        pushad
        movstr dword [A0], "DISCONNECTED FROM THE SERVER"
        calla ShowErrorMenu
        call ExitIfInDirectMode
        call InitJoinGameMenu
        mov  eax, joinGameMenu
        xor  edx, edx
        call ReturnToMenu       ; make sure we land where we're supposed to
        mov  byte [choosingTeam], 0
        calla ExitChooseTeams   ; in case we interrupt choose team menu
        popad
        retn


; MiniMenuLoop
;
; Essential parts of menu loop - menu loop in a nutshell.
;
MiniMenuLoop:
        call DrawModalDialog
        calla FlipInMenu
        mov  ax, 4              ; try to set a longer delay if faster memory to video copy

        cmp  word [videoSpeedIndex], 50
        jg   short .pause
        mov  ax, 2

.pause:
        cmp  [menuCycleTimer], ax
        jl   short .pause

        mov  word [menuCycleTimer], 0

        calla SWOS_ReadGamePort
        calla CheckControls
        calla WaitRetrace
        movzx ecx, word [PIT_countdown]
        mov  al, 110110b
        jmpa ProgramPIT


; JoinGameModalDialogProc [called from C++]
;
; in:
;       al - status code:
;           -1 = done, error
;            0 = not done
;            1 = done, ok
;     edx -> error string (if error status)
;
; out:
;      al - true  - user requesting dispensing of dialog
;           false - stay blocked
;
; While a blocking operation is being done, draw corresponding modal dialog
; based on status code. Re-initialize dialog on code changes. Return true if
; requesting dismissal. If error, stay in error state until dismissal.
;
extern DrawModalDialog, disabledInputCycles
JoinGameModalDialogProc:
        pushad
        mov  bl, [lastModalState]
        mov  [lastModalState], al

        cmp  al, 1
        jz   .out               ; also return true since we're done

        cmp  al, bl
        jz   .draw_modal

        cmp  bl, -1
        jnz  .reinitialize

        mov  byte [lastModalState], -1  ; make sure we stay in error state
        jmp  .draw_modal


.reinitialize:
        test edx, edx           ; set which string we're showing
        mov  byte [disabledInputCycles], 15
        mov  dword [errorModalDialogInitData + 10], failedToConnectStrTable
        jz   .set_init_data

        mov  esi, eax
        mov  dword [errorModalDialogInitData + 10], serverErrorMessageStrTable
        mov  ebx, serverErrorMessageLen - 1
        mov  eax, serverErrorMessage
        mov  edi, eax
        calla swos_libc_strncpy_, ecx
        mov  byte [eax], 0
        mov  eax, edi
        call strupr
        mov  eax, esi

.set_init_data:
        test al, al
        mov  eax, errorModalDialogInitData
        jnz  .init_dialog
        mov  eax, modalDialogInitData

.init_dialog:
        call InitModalDialog

.draw_modal:
        call MiniMenuLoop
        xor  eax, eax
        mov  al, [lastModalState]
        test al, al
        jz   .out

        mov  ax, word [fire]            ; check fire only if we're showing error dialog
        mov  word [fire], 0             ; prevent from exiting menu
        test ax, ax
        jz   .out

        mov  al, [disabledInputCycles]
        test al, al
        jnz  .out

        mov  byte [lastModalState], 0   ; reset this so we don't remain in error state forever ;)
        call ExitIfInDirectMode         ; if we failed to connect while in direct connect mode, exit SWOS
        mov  al, 1

.out:
        mov  [esp + 28], eax
        popad
        retn


section .data

    ;
    ; game lobby menu
    ;

    StartMenu gameLobbyMenu, InitGameLobbyMenu, SaveCurrentGLEntry, GameLobbyBeforeDraw, 22
        ; [0] title
        StartEntry  87, 0, 130, 15
            EntryColor  0x17
            EntryString 0, "MULTIPLAYER GAME SETUP"
        EndEntry

        ; [1] cancel game
        StartEntry 164, 190, 74, 10, cancelGame
            NextEntries 40, 2, 21, -1
            EntryColor  12
            EntryString 0, "CANCEL GAME"
            OnSelect    ExitGameLobbyMenu
        EndEntry

        ; [2] start game
        StartEntry 238, 190, 74, 10, startGame
            NextEntries gameLobbyMenu_cancelGame, -1, 52, -1
            EntryColor  9
            EntryString 0, "START GAME"
            OnSelect    OnStartGame
            DownSkip    1, 0    ; go left
        EndEntry

        ; [3] "connected players:"
        %assign player_entry_width 130
        %assign start_y 16
        StartEntry 1, start_y, player_entry_width, 12
            EntryColor 9
            EntryString 0, "CONNECTED PLAYERS:"
        EndEntry

        ; [4] "teams:"
        StartEntry 10 + player_entry_width, start_y, player_entry_width, 12
            EntryColor 9
            EntryString 0, "TEAMS:"
        EndEntry

        ; [5] "go:"
        StartEntry 19 + 2 * player_entry_width, start_y, 25, 12
            EntryColor 9
            EntryString 0, "GO:"
        EndEntry

        ; template for player names
        StartTemplateEntry
        StartEntry 1, start_y + 13, player_entry_width, 8
            EntryString 0, 0
            EntryColor 13
        EndEntry

        MarkNextEntryOrdinal glPlayerNamesStart
        ; [6-13] joined player names
        %assign y start_y + 13
        %assign index glPlayerNamesStart
        %assign prev -1
        %rep MAX_PLAYERS
            %assign next index + 1
            %if index >= glPlayerNamesStart + MAX_PLAYERS - 1
                %assign next 40
            %endif
            StartEntry 1, y, player_entry_width, 8
                NextEntries -1, index + MAX_PLAYERS, prev, next
            EndEntry
            %assign y y + 9
            %assign prev index
            %assign index index + 1
        %endrep
        RestoreTemplateEntry
        MarkNextEntryOrdinal glPlayerNamesEnd

        ; template for player teams
        StartTemplateEntry
        StartEntry 10 + player_entry_width, start_y + 22, player_entry_width, 8
            EntryColor 12
            EntryString 0, 0
            OnSelect 0
        EndEntry

        MarkNextEntryOrdinal glPlayerTeamNamesStart
        ; [14-21] joined player teams
        %assign y start_y + 13
        %assign index glPlayerTeamNamesStart
        %assign prev -1
        %assign right glPlayerTeamNamesStart + MAX_PLAYERS
        %rep MAX_PLAYERS
            %assign next index + 1
            %if index >= glPlayerTeamNamesStart + MAX_PLAYERS - 1
                %assign next 42
            %endif
            StartEntry 10 + player_entry_width, y, player_entry_width, 8
            NextEntries index - glPlayerTeamNamesStart + glPlayerNamesStart, right, prev, next
            EndEntry
            %assign prev index
            %assign index index + 1
            %assign y y + 9
        %endrep
        RestoreTemplateEntry
        MarkNextEntryOrdinal glPlayerTeamNamesEnd

        ; joined players go checkbox template
        StartTemplateEntry
        StartEntry 19 + 2 * player_entry_width, start_y + 13, 25, 8
            EntrySprite2 209
            EntryColor 10
            OnSelect 0
        EndEntry

        MarkNextEntryOrdinal glPlayerGoCheckboxesStart
        ; [22-29] joined players go checkboxes
        %assign y start_y + 13
        %assign index glPlayerGoCheckboxesStart
        %rep MAX_PLAYERS
            StartEntry 19 + 2 * player_entry_width, y, 25, 8
                %if index == glPlayerGoCheckboxesStart
                    OnSelect ToggleReadyState
                    BeforeDraw UpdateReadyState
                    NextEntries glPlayerTeamNamesStart, -1, -1, 42
                %endif
            EndEntry
            %assign index index + 1
            %assign y y + 9
        %endrep
        RestoreTemplateEntry
        MarkNextEntryOrdinal glPlayerGoCheckboxesEnd

        ; [30] "chat:"
        %assign chat_x 1
        %assign chat_y 100
        %assign chat_width 161
        StartEntry chat_x, chat_y, chat_width, 8
            EntryString 0, "CHAT:"
            EntryColor 14
        EndEntry

        ; chat lines template
        StartTemplateEntry
        StartEntry
            EntryString 1 << 15, 0
            InvisibleEntry
        EndEntry

        MarkNextEntryOrdinal glChatLines
        ; [31-39] chat lines
        %assign y chat_y + 9
        %rep 9
            StartEntry chat_x, y, chat_width, 8
            EndEntry
            %assign y y + 9
        %endrep
        RestoreTemplateEntry

        ; [40] chat input
        StartEntry chat_x, y, chat_width, 10, chatInput
            EntryColor 13
            EntryString 1 << 15, -1
            OnSelect ChatLineInput
            NextEntries -1, gameLobbyMenu_cancelGame, glPlayerNamesEnd - 1, -1
        EndEntry

        ; options
        ; [41] join as text
        StartEntry 164, chat_y, 82, 11
            EntryColor 9
            EntryString 0, "JOIN AS"
        EndEntry

        MarkNextEntryOrdinal glOptionsStart
        ; [42] player/watcher
        StartEntry 251, chat_y, 54, 11, playerWatcher
            EntryColor 13
            StringTable 0, playerWatcherStrTable
            OnSelect ChangePlayerWatcher
            NextEntries -1, -1, glPlayerGoCheckboxesStart, 44
        EndEntry

        ; [43] game length text
        StartEntry 164, chat_y + 13, 82, 11
            EntryColor 9
            EntryString 0, aGameLength
        EndEntry

        ; [44] game length
        StartEntry 251, chat_y + 13, 54, 11, optGameLength
            EntryColor 13
            StringTable 0, gameLengthStrTable
            OnSelect ChangeGameLengthAndNotify
            NextEntries -1, -1, gameLobbyMenu_playerWatcher, 46
        EndEntry

        ; [45] pitch type text
        StartEntry 164, chat_y + 2 * 13, 82, 11
            EntryColor 9
            EntryString 0, aPitchType
        EndEntry

        ; [46] pitch type
        StartEntry 251, chat_y + 2 * 13, 54, 11, optPitchType
            EntryColor 13
            StringTable 0, pitchTypeStrTable
            OnSelect ChangePitchTypeAndNotify
            NextEntries -1, -1, gameLobbyMenu_optGameLength, 48
        EndEntry

        ; [47] substitutes text
        StartEntry 164, chat_y + 3 * 13, 82, 11
            EntryColor 9
            EntryString 0, aSubstitutes
        EndEntry

        ; [48] number of substitutes
        StartEntry 251, chat_y + 3 * 13, 11, 11, optNumSubstitutes
            EntryColor 13
            EntryString 0, -1
            OnSelect ChangeNumSubstitutesAndNotify
            NextEntries -1, 50, gameLobbyMenu_optPitchType, 52
        EndEntry

        ; [49] substitutes from text
        StartEntry 263, chat_y + 3 * 13, 30, 11
            EntryColor 9
            EntryString 0, aFrom
        EndEntry

        ; [50] max substitutes
        StartEntry 294, chat_y + 3 * 13, 11, 11, optMaxSubstitutes
            EntryColor 13
            EntryString 0, -1
            NextEntries gameLobbyMenu_optNumSubstitutes, -1, \
                gameLobbyMenu_optPitchType, 52
            OnSelect ChangeMaxSubstitutesAndNotify
        EndEntry

        ; [51] auto save replays text
        StartEntry 164, chat_y + 4 * 13, 82, 11
            EntryColor 9
            EntryString 0, "SAVE REPLAYS"
        EndEntry

        ; [52] auto save replays on/off
        StartEntry 251, chat_y + 4 * 13, 54, 11, optAutoSaveReplays
            EntryColor 13
            OnSelect ToggleSaveAutoReplays
            StringTable 0, autoSaveReplaysTable
            NextEntries -1, -1, gameLobbyMenu_optNumSubstitutes, 54
        EndEntry

        ; [53] skip frames text
        StartEntry 164, chat_y + 5 * 13, 82, 11
            EntryColor 9
            EntryString 0, "SKIP FRAMES"
        EndEntry

        ; [54] decrease skip frames button
        StartEntry 251, chat_y + 5 * 13, 16, 11, optDecreaseFrameSkip
            EntryColor 13
            EntryString 0, aMinus
            OnSelect DecreaseSkipFramesAndNotify
            NextEntries -1, 56, gameLobbyMenu_optAutoSaveReplays, gameLobbyMenu_startGame
        EndEntry

        ; [55] skip frames
        StartEntry 268, chat_y + 5 * 13, 20, 11, optSkipFrames
            EntryColor 9
            EntryNumber 0, 0
        EndEntry

        ; [56] increase skip frames button
        StartEntry 289, chat_y + 5 * 13, 16, 11, optIncreaseFrameSkip
            EntryColor 13
            EntryString 0, aPlus
            OnSelect IncreaseSkipFramesAndNotify
            NextEntries gameLobbyMenu_optDecreaseFrameSkip, -1, \
                gameLobbyMenu_optAutoSaveReplays, gameLobbyMenu_startGame
        EndEntry

    EndMenu

; entries that have to be permanently disabled when we're entering this menu as clients
entriesToDisable:
        db gameLobbyMenu_optGameLength
        db gameLobbyMenu_optPitchType
        db gameLobbyMenu_optNumSubstitutes
        db gameLobbyMenu_optMaxSubstitutes
        db gameLobbyMenu_startGame
        db gameLobbyMenu_optDecreaseFrameSkip
        db gameLobbyMenu_optIncreaseFrameSkip
        db -1


section .text


; GetGameLobbyMenu
;
; External accessor for game lobby menu.
;
global GetGameLobbyMenu
GetGameLobbyMenu:
        mov  eax, gameLobbyMenu
        retn


; DisplayErrorAndExit
;
; Unpatch everything and make a switch from game to menu display.
; Then show error menu and wait for player to press exit.
; Only called in direct connect mode.
;
extern ShowErrorAndQuit, FinishMultiplayerGame
DisplayErrorAndExit:
        calla FadeOutToBlack
        calla LoadFillAndSwtitle
        mov  eax, [linAdr384k]
        mov  [vsPtr], eax
        mov  word [screenWidth], 320
        mov  word [cameraX], 0
        mov  word [cameraY], 0
        call UnpatchAfterSettingTeams   ; required to regain user input
        call FinishMultiplayerGame
        movstr eax, "GAME INTERRUPTED"
        jmp  ShowErrorAndQuit


; InitGameLobbyMenu
;
; Game lobby menu OnInit function.
;
extern CreateNewGame, CanGameStart, GetFreshMPOptions
InitGameLobbyMenu:
        call GetDirectGameName
        mov  al, [eax]
        mov  edi, ReturnToMPMenuAfterTheGame
        test al, al
        jz   .not_direct_mode

        mov  edi, DisplayErrorAndExit

.not_direct_mode:
        mov  byte [syncDialogInitialized], 0
        push byte gameLobbyMenu_chatInput
        pop  dword [D0]
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  esi, [esi + MenuEntry.string]
        mov  [esi], byte 0
        sub  esp, MP_Options_size
        mov  eax, esp
        call GetFreshMPOptions
        mov  edx, UpdateGameLobby
        movzx ecx, byte [weAreTheServer]
        push ecx
        push ReturnToGameLobbyAfterTheGame
        mov  ecx, edi
        call CreateNewGame
        add  esp, MP_Options_size
        mov  byte [isReady], 0                              ; not ready by default
%ifdef DEBUG
        call ToggleReadyState                               ; except in debug version
%endif
        mov  word [playerOrWatcher], 0                      ; player (not watcher) by default
        call SetNumSubstitutesText
        mov  dword [D0], gameLobbyMenu_optAutoSaveReplays   ; disable auto-save replays for now
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  word [esi + MenuEntry.isDisabled], 1
        mov  word [esi + MenuEntry.backAndFrameColor], 9
        call DisableServerOnlyEntries
.out:
        retn


SaveCurrentGLEntry:
        mov  eax, [g_currentMenu + Menu.currentEntry]
        test eax, eax
        jz   .out

        mov  eax, [eax + MenuEntry.ordinal]
        mov  [gameLobbySelectedEntry], eax

.out:
        retn


; DisableServerOnlyEntries
;
; Disable certain menu entries that only host is allowed to change (game options).
; Must be called from init and after draw functions.
;
DisableServerOnlyEntries:
        push byte gameLobbyMenu_optAutoSaveReplays          ; disable auto-save replays for now
        pop  dword [D0]
        calla CalcMenuEntryAddress
        mov  eax, [A0]
        mov  word [eax + MenuEntry.isDisabled], 1
        mov  dword [eax + MenuEntry.backAndFrameColor], 9

        mov  al, [weAreTheServer]
        test al, al
        jnz  InitGameLobbyMenu.out
        mov  esi, entriesToDisable                          ; if not server disable option entries

.disable_next_entry:
        lodsb
        cmp  al, -1
        jz   InitGameLobbyMenu.out
        cbw
        mov  [D0], ax
        calla CalcMenuEntryAddress
        mov  eax, [A0]
        mov  word [eax + MenuEntry.isDisabled], 1
        mov  dword [eax + MenuEntry.backAndFrameColor], 9
        jmp  .disable_next_entry


; FixSelectedEntry
;
; After refreshing player list check if currently selected player may have been
; disconnected or left on their own. If so, move cursor to entry above.
; Must not modify edi.
;
FixSelectedEntry:
        mov  ecx, g_currentMenu
        mov  esi, [ecx + Menu.currentEntry]
        test esi, esi
        jz   short .out

        cmp  word [esi + MenuEntry.isInvisible], 0
        jz   short .out

        movsx eax, byte [esi + MenuEntry.upEntry]
        test eax, eax                               ; this should never happen, as there should always be
        js   short .out                             ; at least 1 player above - us, but let's be safe

        lea  ebx, [8 * eax - Menu_size]
        shl  eax, 6
        sub  eax, ebx                               ; eax = entry index * 56 (MenuEntry size) + Menu header size
        add  eax, ecx                               ; eax -> up entry
        mov  dword [ecx + Menu.currentEntry], eax

.out:
        retn


; UpdateGameLobby [called from C++]
;
; in:
;     eax -> LobbyState structure (described in mplayer.h)
;
; Update entire game lobby screen based on informations received from the
; network module. Called each frame.
;
extern GetCurrentTeamId, ApplyMPOptions
UpdateGameLobby:
        pushad
        cmp  byte [choosingTeam], 0
        jnz  .out

        mov  edi, eax                                       ; edi will point to passed infos
        mov  ecx, [edi + LobbyState.numPlayers]             ; ecx - number of players

        push byte glPlayerNamesStart
        pop  edx
        mov  [D0], edx                                      ; edx - current player entry number
        push edx
        calla CalcMenuEntryAddress
        pop  edx
        mov  ebp, [A0]                                      ; ebp -> current player name entry
        lea  esi, [ebp + (glPlayerTeamNamesStart - glPlayerNamesStart) * MenuEntry_size]
        push esi                                            ; esi -> current player team name entry
        lea  ebx, [ebp + (glPlayerGoCheckboxesStart - glPlayerNamesStart) * MenuEntry_size]
                                                            ; ebx -> current player ready icon entry

        ; set each player's name, team and ready icon
.players_loop:
        xor  eax, eax
        dec  ecx
        mov  [ebp + MenuEntry.string], dword 0              ; assume no player
        mov  [esi + MenuEntry.string], dword 0
        mov  [esi + MenuEntry.onSelect], dword 0
        mov  [ebp + MenuEntry.isInvisible], dword -1        ; set both invisible and disabled
        mov  [ebx + MenuEntry.isInvisible], dword -1
        mov  [esi + MenuEntry.isInvisible], dword -1
        mov  [ebx + MenuEntry.sprite2], word 209            ; assume not ready
        mov  [ebx + MenuEntry.backAndFrameColor], word 10   ; red
        js   .next

        mov  eax, [edi + LobbyState.playerNames + 4 * (edx - glPlayerNamesStart)]
        mov  [ebp + MenuEntry.isInvisible], dword 0         ; make it visible and enable it
        mov  [esi + MenuEntry.isInvisible], dword 0
        mov  [ebx + MenuEntry.isInvisible], dword 0
        mov  [ebp + MenuEntry.string], eax                  ; set the name string
        mov  eax, [edi + LobbyState.playerFlags + 4 * (edx - glPlayerNamesStart)]
        push eax
        and  eax, 2
        shr  eax, 1
        sub  [ebx + MenuEntry.sprite2], ax                  ; 208 = ready, 209 = not ready
        shl  eax, 2
        add  [ebx + MenuEntry.backAndFrameColor], ax        ; 14 for green if ready
        pop  eax
        and  eax, 1
        lea  eax, [eax * 2 + 11]                            ; color = 11 for player, 13 for watcher
        mov  [ebp + MenuEntry.backAndFrameColor], ax
        mov  eax, [edi + LobbyState.playerTeamsNames + 4 * (edx - glPlayerNamesStart)]
        mov  [esi + MenuEntry.string], eax                  ; set the team name string
        test eax, eax
        jz   .team_not_chosen

        mov  al, [eax]
        test al, al
        jnz  .next

.team_not_chosen:
        mov dword [esi + MenuEntry.string], aTeamNotChosen

.next:
        inc  edx
        add  ebp, MenuEntry_size
        add  esi, MenuEntry_size
        add  ebx, MenuEntry_size
        cmp  edx, byte glPlayerNamesEnd
        jb   .players_loop

        call FixSelectedEntry

        mov  word [D0], glPlayerGoCheckboxesStart           ; fix our player, they're always at index 0
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  dword [esi + MenuEntry.onSelect], ToggleReadyState
        pop  esi
        mov  dword [esi + MenuEntry.onSelect], ChooseTeam   ; enable our player to select team
        call GetCurrentTeamId
        cmp  eax, -1
        jz   .set_choose_team

        mov  eax, [esi + MenuEntry.string]
        test eax, eax
        jz   .set_choose_team
        mov  al, [eax]
        test al, al
        jnz  .apply_options

.set_choose_team:
        mov  dword [esi + MenuEntry.string], aChooseTeam    ; "choose team" if not chosen already
        mov  word [esi + MenuEntry.backAndFrameColor], 10   ; mark the entry as red

.apply_options:
        mov  ebp, edi
        lea  eax, [edi + LobbyState.MP_Options]
        call ApplyMPOptions

        xor  ecx, ecx                                       ; set the chat lines
        push byte glChatLines
        pop  eax
        mov  [D0], eax
        calla CalcMenuEntryAddress
        mov  esi, [A0]

        ; fill chat lines
.next_line:
        mov  dword [esi + MenuEntry.isInvisible], -1        ; assume no chat line
        mov  byte [esi + MenuEntry.stringFlags], 0
        mov  eax, [ebp + 4 * ecx + LobbyState.chatLines]
        test eax, eax
        jz   .skip_line

        mov  bl, [eax]
        test bl, bl
        jz   .skip_line

        mov  dword [esi + MenuEntry.isInvisible], 0         ; we got the line
        mov  [esi + MenuEntry.string], eax
        mov  al, [ebp + ecx + LobbyState.chatLineColors]
        mov  [esi + MenuEntry.stringFlags], al

.skip_line:
        inc  ecx
        add  esi, MenuEntry_size
        cmp  ecx, MAX_CHAT_LINES
        jb   .next_line

.out:
        popad
        retn


extern UpdateMPOptions
ChangeGameLengthAndNotify:
        calla ChangeGameLength

NotifyOptionsChange:
        sub  esp, MP_Options_size
        mov  eax, esp
        call GetFreshMPOptions
        mov  eax, esp
        call UpdateMPOptions
        add  esp, MP_Options_size
        retn


IncDecSkipFramesAndNotify:
        call GetSkipFrames
.opByte:
        dec  eax
        jns  .set_frames

        inc  eax

.set_frames:
        call SetSkipFrames
        push byte gameLobbyMenu_optSkipFrames
        pop  dword [D0]
        calla CalcMenuEntryAddress
        call GetSkipFrames
        mov  esi, [A0]
        mov  [esi + MenuEntry.number], eax
        jmp  NotifyOptionsChange

DecreaseSkipFramesAndNotify:
        mov  byte [IncDecSkipFramesAndNotify.opByte], 0x48
        jmp  IncDecSkipFramesAndNotify

IncreaseSkipFramesAndNotify:
        mov  byte [IncDecSkipFramesAndNotify.opByte], 0x40
        jmp  IncDecSkipFramesAndNotify

ChangePitchTypeAndNotify:
        calla ChangePitchType
        jmp NotifyOptionsChange


extern AddChatLine
ChatLineInput:
        mov  word [D0], MAX_CHAT_LINE_LENGTH + 1
        mov  esi, [A5]
        mov  ebx, [esi + MenuEntry.string]
        push ebx
        mov  dword [A0], ebx
        mov  word [inputingTextOk], 0
        calla InputText
        pop  ebx
        mov  eax, [D0]
        test ax, ax
        jnz  .out

        mov  eax, ebx
        call AddChatLine

.out:
        mov  eax, ebx
        mov  [eax], byte 0
        retn


EnterInputTextHook:
        cmp  dword [A0], ChatLineInput      ; is this our entry?
        jnz  .out

        cmp  [lastKey], word 0x1c           ; yes! check if enter was pressed
        jnz  .out

        or   eax, -1                        ; bingo! set eax to -1 as if fire was pressed
        retn

.out:
        mov  ax, [shortFire]
        or   ax, ax
        retn


InputTextOnEnterPatch:
        calla EnterInputTextHook
        nop
        nop
InputTextOnEnterPatchSize equ $ - InputTextOnEnterPatch


; in:
;     esi -> data to patch with
;
PatchMenuProc:
        mov  edi, MenuProc + 0x114
        push byte InputTextOnEnterPatchSize
        pop  ecx
        cld
        rep  movsb
        retn


EnableInputTextOnEnter:
        mov  esi, InputTextOnEnterPatch
        jmp  PatchMenuProc


DisableInputTextOnEnter:
        mov  esi, EnterInputTextHook.out
        jmp  PatchMenuProc


; choose this players team for a multiplayer game
global ChooseTeam
ChooseTeam:
        xor  eax, eax
        mov  [A1], eax      ; no need for on select function
        inc  eax
        mov  [D2], eax      ; show club teams
        mov  [D3], eax      ; show national teams
        mov  [D4], eax      ; show original custom teams
        movstr dword [A0], "CHOOSE MULTIPLAYER GAME TEAM"
        mov  byte [choosingTeam], 1
        calla ChooseTeamsDialog
        mov  byte [choosingTeam], 0
        mov  eax, [A0]
        mov  edx, [D0]      ; selected team country number or -1 if aborted
        cmp  dx, -1
        jz   .out

        mov  edx, [eax]     ; selected team unique number
        call SetTeam
.out:
        retn


; fire triggered on num. substitutes
ChangeNumSubstitutesAndNotify:
        call GetMaxSubstitutes
        test eax, eax
        jz   .out

        mov  ebx, eax       ; ebx = max substitutes
        call GetNumSubstitutes
        inc  eax
        cmp  ebx, eax
        jnb  .in_range

        mov  eax, 1         ; num. substitutes = 1 if it's bigger than max

.in_range:
        call SetNumSubstitutes
        call SetNumSubstitutesText
        calla DrawMenuItem

.out:
        jmp  NotifyOptionsChange


; fire triggered on max substitutes
ChangeMaxSubstitutesAndNotify:
        call GetMaxSubstitutes
        inc  eax
        cmp  eax, 5
        jbe  .in_range

        xor  eax, eax
        call SetMaxSubstitutes

.in_range:
        cmp  eax, 1
        ja   .skip_fixing_subs

        call SetNumSubstitutes

.skip_fixing_subs:
        call SetMaxSubstitutes
        call SetNumSubstitutesText
        calla DrawMenuItem
        mov  dword [D0], gameLobbyMenu_optNumSubstitutes
        calla CalcMenuEntryAddress
        mov  eax, [A0]
        mov  [A5], eax
        calla DrawMenuItem
        jmp  NotifyOptionsChange


SetSkipFramesText:
        push byte gameLobbyMenu_optSkipFrames
        pop  dword [D0]
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        call GetSkipFrames
        mov  [esi + MenuEntry.number], eax
        retn


SetNumSubstitutesText:
        push byte gameLobbyMenu_optNumSubstitutes
        pop  dword [D0]
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  eax, [esi + MenuEntry.string]
        mov  dword [A1], eax
        call GetNumSubstitutes
        mov  [D0], eax
        calla Int2Ascii
        push byte gameLobbyMenu_optMaxSubstitutes
        pop  dword [D0]
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  eax, [esi + MenuEntry.string]
        mov  dword [A1], eax
        call GetMaxSubstitutes
        mov  [D0], eax
        jmpa Int2Ascii


ToggleSaveAutoReplays:
        xor  word [autoSaveReplays], 1
        jmpa DrawMenuItem


; called before creating Multiplayer menu
extern GetMPOptions, EnableAdditionalInputKeys, DisableAdditionalInputKeys
PatchMenu:
        call EnableInputTextOnEnter
        call EnableAdditionalInputKeys
        mov  byte [ChangePitchType + 0x19], 0xff    ; eliminate seasonal
        retn


; deinitialization of Multiplayer menu
UnpatchMenu:
        call DisableInputTextOnEnter
        call DisableAdditionalInputKeys
        mov  byte [ChangePitchType + 0x19], 0xfe    ; restore pitch type table
        retn


; see if we can enable start game button
GameLobbyBeforeDraw:
        call SaveCurrentGLEntry
        call SetNumSubstitutesText
        call SetSkipFramesText
        call CanGameStart
        neg  eax
        sbb  eax, eax
        push eax
        and  eax, 5
        lea  ecx, [eax + 9]
        and  eax, 1
        shl  eax, 13
        mov  edi, eax
        mov  word [D0], gameLobbyMenu_startGame
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        pop  eax
        not  eax
        mov  word [esi + MenuEntry.isDisabled], ax ; disable/enable start game button
        mov  [esi + MenuEntry.backAndFrameColor], cx
        mov  [esi + MenuEntry.stringFlags], di

        test eax, eax
        jz   .fix_chat_input_line

        mov  edi, [A6]      ; check if start game is selected while we disable it
        mov  eax, [edi + Menu.currentEntry]
        cmp  eax, esi
        jnz  .fix_chat_input_line

        push byte gameLobbyMenu_cancelGame  ; move focus to cancel game button
        pop  dword [D0]
        calla SetCurrentEntry

.fix_chat_input_line:
        cmp  word [inputingTextOk], 0
        jnz  .chat_input_ok

        push byte gameLobbyMenu_chatInput
        pop  dword [D0]
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  eax, [esi + MenuEntry.string]
        mov  [eax], byte 0  ; prevent "STDMENUTEXT" from showing up after choose team menu

.chat_input_ok:
        call DisableServerOnlyEntries
        retn


UpdateReadyState:
        mov  esi, [A5]
        mov  [esi + MenuEntry.sprite2], word 208            ; assume ready
        mov  [esi + MenuEntry.backAndFrameColor], word 14   ; check, background color = green
        xor  eax, eax
        mov  al, [isReady]
        test eax, eax
        jnz  .out

        inc  word [esi + MenuEntry.sprite2]                 ; uncheck, background color = red
        mov  [esi + MenuEntry.backAndFrameColor], word 10

.out:
        retn


extern SetPlayerReadyState
ToggleReadyState:
        xor  byte [isReady], 1
        movzx eax, byte [isReady]
        call SetPlayerReadyState
        jmp  UpdateReadyState


; ExitGameLobbyMenu
;
; Show confirmation dialog when leaving multiplayer game. If we're called from direct mode
; exit SWOS if player chooses so, otherwise go back to previous menu.
;
extern DisbandGame
ExitGameLobbyMenu:
        call GetDirectGameName
        mov  al, [eax]
        test al, al
        movstr dword [A0], "EXIT GAME"
        mov  dword [A1], exitGamePrompt
        push eax
        jnz  .show_prompt

        cmp  byte [weAreTheServer], 0
        movstr dword [A0], "DISBAND MULTIPLAYER ROOM"
        mov  dword [A1], disbandGamePrompt
        jnz  .show_prompt

        movstr dword [A0], "LEAVE MULTIPLAYER GAME"
        mov  dword [A1], leaveGamePrompt

.show_prompt:
        mov  dword [A2], aContinue
        mov  dword [A3], aAbort
        calla DoContinueAbortMenu
        pop  ebx
        jnz  .out

        call DisbandGame                ; user is leaving the game, disband in any case
        test bl, bl
        jz   .back_to_prev_menu

        call ExitSWOS                   ; it's direct connect mode, exit SWOS

.back_to_prev_menu:
        calla SetExitMenuFlag           ; not direct mode, return to previous menu

        cmp  byte [weAreTheServer], 0
        jnz  .out

        call InitJoinGameMenu

.out:
        retn


extern SetPlayerOrWatcher
ChangePlayerWatcher:
        xor  eax, eax
        mov  ax, [playerOrWatcher]
        inc  eax
        cmp  eax, 2
        jb   .out

        xor  eax, eax
.out:
        mov  [playerOrWatcher], ax
        call SetPlayerOrWatcher
        retn


; OnStartGame
;
; Play match was selected in game lobby. Show play match menu for players to
; set up their teams.
;
extern SetupTeams
OnStartGame:
        WriteToLog "Play match selected... going to synchronization..."
        mov  word [showingCpuTeams], 0          ; just in case
        mov  eax, SyncModalDialogProc
        mov  edx, ShowPlayMatchMenu
        mov  ecx, eax
        push PlayMatchMenuTimeout
        call SetupTeams                         ; let's rock'n'roll
        retn


; SyncModalDialogProc [called from C++]
;
; Draw modal dialog while player team synchronization is being done.
;
SyncModalDialogProc:
        pushad
        cmp  byte [syncDialogInitialized], 0
        jnz  .menu_loop

        mov  dword [modalDialogStrTable], syncingPleaseWaitStrTable
        mov  eax, modalDialogInitData
        call InitModalDialog
        mov  byte [syncDialogInitialized], 1

.menu_loop:
        call MiniMenuLoop
        popad
        retn


; HookFindTeamInCache
;
; in:
;     ecx -> get current team function
;
; Patch FindTeamsInCache since it blindly returns team from selectedTeams based on team id.
; That causes horrible problems when both players are using same team. Make it return team of
; player that's currently in control -- callback function will provide us with that.
; Make sure esi is not touched.
;
HookFindTeamInCache:
        mov  dword [ReturnCurrentTeam + 1], ecx

        mov  al, [FindTeamInCache]
        mov  [savedFindTeamInCacheBytes], al
        mov  eax, [FindTeamInCache + 1]
        mov  [savedFindTeamInCacheBytes + 1], eax

        mov  byte [FindTeamInCache], 0xe9
        mov  eax, ReturnCurrentTeam - 5
        mov  ebx, FindTeamInCache
        sub  eax, ebx
        mov  [FindTeamInCache + 1], eax

        retn


; UnhookFindTeamInCache
;
; Return FindTeamInCache to its original state (since it won't work like this
; during the menus).
;
UnhookFindTeamInCache:
        mov  al, [savedFindTeamInCacheBytes]
        mov  [FindTeamInCache], al
        mov  eax, [savedFindTeamInCacheBytes + 1]
        mov  [FindTeamInCache + 1], eax
        retn


; ReturnCurrentTeam
;
; Run in place of FindTeamInCache, and always return team of the controlling player
; with a help of a patched in callback.
;
ReturnCurrentTeam:
        db 0xb8, 0x90, 0x90, 0x90, 0x90
        call eax                    ; it will only ever return 0 or 1
        mov  word [D0], ax
        neg  eax
        and  eax, TEAM_SIZE
        lea  eax, [eax + g_selectedTeams]
        mov  [A0], eax
        mov  word [D1], 0
        xor  eax, eax
        retn


; ShowPlayMatchMenu [called from C++]
;
; in:
;     eax -> team1
;     edx -> team2
;     ecx -> get current team function
;
; Called when we're entering menu for players to set up their teams after successful
; synchronization. Do all the necessary patching of play match menu for multiplayer to work.
;
extern lastFireState
ShowPlayMatchMenu:
        pushad
        mov  byte [lastFireState], 0
        mov  byte [disabledInputCycles], -1 ; signal to ask keys from network
        mov  word [player2ClearFlag], -1    ; block player2 input
        mov  word [player1ClearFlag], 0     ; just in case
        mov  esi, playMatchMenu
        mov  dword [esi + Menu.afterDraw], FixPlayMatchMenuAfterDraw
        mov  dword [esi + Menu.onDraw], FixPlayMatchMenu
        mov  dword [esi + 0x134], HookPlayMatchSelected
        mov  dword [esi + 0x10a], HookExitPlayMatch

        mov  byte [eax + 4], 2
        mov  byte [edx + 4], 2              ; hopefully "COACH" won't show up anymore

        mov  [A1], eax
        mov  [A2], edx
%ifdef DEBUG
        lea eax, [eax + 5]
        lea edx, [edx + 5]
        WriteToLog "Left team is %s, right team is %s", eax, edx
%endif

        call HookFindTeamInCache
        movzx eax, byte [g_joyKbdWord]
        push byte 1
        cmp  al, convertControlsTableLen
        pop  ebx
        jae  .set_controls

        mov  ebx, eax

.set_controls:
        mov  al, [g_joyKbdWord]                 ; we used to save g_joyKbdWord by push/pop but in case of network
        mov  [savedJoyKbdWord], al              ; error stack might get chopped off, so this is safer
        xchg al, [convertControlsTable + ebx]
        mov  [g_joyKbdWord], al
        WriteToLog "g_joyKbdWord set to %d for menus.", eax

        mov  byte [syncDialogInitialized], 0    ; we'll need this later

        calla InitAndPlayGame
        call UnpatchAfterSettingTeams

.out:
        popad
        retn


; UnpatchAfterSettingTeams
;
; Clean up after returning from play match menu.
;
global UnpatchAfterSettingTeams
UnpatchAfterSettingTeams:
        mov  esi, playMatchMenu
        mov  dword [esi + 0x10a], ExitPlayMatch
        mov  dword [esi + 0x134], PlayMatchSelected
        mov  dword [esi + Menu.afterDraw], PlayMatchAfterDraw
        mov  dword [esi + Menu.onDraw], 0
        mov  byte [disabledInputCycles], 0
        mov  word [player2ClearFlag], 0
        mov  word [keyCount], 0
        mov  al, [savedJoyKbdWord]
        mov  [g_joyKbdWord], al
        jmp  UnhookFindTeamInCache


disabledPlayMatchMenuEntries:
        db 6, 7, 10, -1     ; player, view opponent, edit tactics

; FixPlayMatchMenuAfterDraw/FixPlayMatchMenu
;
; Make necessary changes on play match menu to accommodate multiplayer mode.
; Executing as on draw menu proc, and also on after draw to negate changes done
; by SWOS.
;
FixPlayMatchMenuAfterDraw:
        calla PlayMatchAfterDraw                    ; let it do it's thing first
FixPlayMatchMenu:
        mov  dword [player1ClearFlag], 0xffff0000   ; keep player 1 clear player 2 set
        mov  esi, disabledPlayMatchMenuEntries

.next:
        xor  eax, eax
        lodsb
        cmp  al, -1
        jz   .out

        mov  word [D0], ax
        calla CalcMenuEntryAddress
        mov  edi, [A0]
        mov  word [edi + MenuEntry.isDisabled], 1
        mov  word [edi + MenuEntry.backAndFrameColor], 9
        jmp  .next

.out:
        retn


; PlayMatchMenuTimeout [called from C++]
;
; in:
;     eax - if true, player that timed out is the server
;
; Called when a timeout occurs while players are setting up their teams (we're in
; play match menu). Depending if timed out player is the server we have two options:
; - if the server is down, go to multiplayer menu, try finding some new games to join
; - if the server is still up, try going back to lobby
;
PlayMatchMenuTimeout:
        pushad
        push eax
        call UnpatchAfterSettingTeams       ; must unpatch first since GetCurrentPlayerTeamIndex() might get called
        movstr dword [A0], "A PLAYER DROPPED"
        calla ShowErrorMenu
        call ExitIfInDirectMode

        pop  eax
        test eax, eax
        jz   .back_to_lobby

        mov  eax, joinGameMenu
        xor  edx, edx
        call ReturnToMenu
        call InitJoinGameMenu
        calla ExitPlayMatch
        jmp  short .out

.back_to_lobby:
        call ReturnToGameLobbyAfterTheGame

.out:
        popad
        retn



; HookPlayMatchSelected
;
; Called when player finishes setting up team and presses play match button.
; Notifies network to go to next state. Relies on SwitchToNextControllingState to
; patch main loop and control status procs to call our hooks.
;
extern SwitchToNextControllingState
HookPlayMatchSelected:
        mov  byte [lastFireState], 0
        mov  word [g_joy1Status], 0
        mov  eax, StartTheGameCallback
        call SwitchToNextControllingState   ; returns zero if we can't start the game yet
        test eax, eax
        jnz  .continue

        retn

.continue:
        cmp  eax, 1
        mov  byte [pl1Fire], 1              ; must set these, as they will be used to determine
        mov  byte [pl2Fire], 0              ; player numbers in teams later
        jz   StartTheGameCallback.out

StartTheGameCallback:
        mov  byte [pl1Fire], 0
        mov  byte [pl2Fire], 1

.out:
        jmpa PlayMatchSelected


; HookExitPlayMatch
;
; Patched in place of on exit play match menu function. Clean up everything then
; proceed as normal.
;
extern GoBackToLobby
HookExitPlayMatch:
        call ReturnToGameLobbyMenu
        jmpa ExitPlayMatch


; ReturnToGameLobbyMenu
;
; Notifies the network that we're leaving play match menu, so it can go back to lobby state.
; Unpatches main loop and control status procs.
;
ReturnToGameLobbyMenu:
        mov  ebx, gameLobbyMenu
        call GoBackToLobby
        call GetPreviousMenu
        cmp  eax, ebx
        jz   .out

        mov  eax, ebx                   ; we are called from something that isn't game lobby menu
        xor  edx, edx
        call ReturnToMenu               ; make sure we return to game lobby
        mov  word [menuFade], 0
        mov  word [contAbortResult], 1  ; abort the possible prompt
        mov  byte [choosingTeam], 0
        call UnpatchAfterSettingTeams   ; stack will be chopped off, we gotta run this manually

.out:
        jmpa ExitChooseTeams


section .data

aJoinGame:
        db "JOIN GAME", 0

disbandGamePrompt:
        db 2
        db "CREATED MULTIPLAYER GAME", 0
        db "IS ABOUT TO BE DISBANDED", 0

leaveGamePrompt:
        db 2
        db "YOU ARE ABOUT TO LEAVE", 0
        db "MULTIPLAYER GAME", 0

exitGamePrompt:
        db 2
        db "YOU ARE ABOUT TO CLOSE", 0
        db "SWOS AND GO BACK TO OS", 0

aConnectionRefused:
        db "CONNECTION REFUSED:", 0

align 4
playerWatcherStrTable:
        dd playerOrWatcher
        dw 0
        dd aPlayer_0
        dd watcherStr
watcherStr:
        db "WATCHER", 0

align 4
autoSaveReplaysTable:
        dd autoSaveReplays
        dw 0
        dd aOff
        dd aOn

modalDialogInitData:
        db 12   ; show OK button: OFF, show sprite(0), animations: ON, loop: ON
        db 6    ; animation delay
        dw 113  ; start sprite
        dw 122  ; end sprite
        dw 32   ; sprite width
        dw 8    ; sprite height
modalDialogStrTable:
        dd connectingPleaseWaitStrTable

errorModalDialogInitData:
        db 7    ; show OK button ON, show bitmap(1), animations: ON, loop: OFF
        db 70   ; animation delay
        dd playerIconErrorAnimTable
        dw 11   ; sprite width
        dw 14   ; sprite height
        dd failedToConnectStrTable

serverErrorMessageStrTable:
        dd 2
        dd aConnectionRefused
        dd serverErrorMessage

aPleaseWait:
        db "PLEASE WAIT", 0

align 4
connectingPleaseWaitStrTable:
        dd 2
        declareStr "CONNECTING..."
        dd aPleaseWait

syncingPleaseWaitStrTable:
        dd 2
        declareStr "SYNCHRONIZING..."
        dd aPleaseWait

failedToConnectStrTable:
        db 1
        db "FAILED TO CONNECT", 0

align 4
playerIconErrorAnimTable:
        dd playerIconError1
        dd playerIconError2
        dd 0

aChooseTeam:
        db "CHOOSE TEAM...", 0
global aTeamNotChosen
aTeamNotChosen:
        db "(TEAM NOT CHOSEN)", 0

; convert g_joyKbdWord values to only keyboard/joypad to enforce Player1StatusProc to read
; joypad too in case of mixed controls (Player2StatusProc won't be called and it's task is
; to read values for player on joypad in case of mixed controls)
align 4
convertControlsTable:
        db 1, 1, 2, 2, 1, 2
convertControlsTableLen equ $ - convertControlsTable

playerIconError1:
incbin "../bitmap/spr0415.bp"

playerIconError2:
incbin "../bitmap/spr0416.bp"


section .bss

align 4, resb 1
gameLobbySelectedEntry:
        resb 4
playerOrWatcher:
        resb 2
isReady:
        resb 1
lastModalState:
        resb 1
autoSaveReplays:
        resb 2
prevRefreshDisabled:
        resb 2
weAreTheServer:
        resb 1
choosingTeam:
        resb 1
serverErrorMessage:
        resb 32
serverErrorMessageLen equ $ - serverErrorMessage
options:
        resb MP_Options_size

syncDialogInitialized:
        resb 1
mainLoopRan:
        resb 1
savedJoyKbdWord:
        resb 1
savedFindTeamInCacheBytes:
        resb 5
