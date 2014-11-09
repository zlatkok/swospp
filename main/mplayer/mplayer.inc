; keep in sync with mplayer.h

NICKNAME_LEN            equ 20
MAX_PLAYERS             equ  8
MAX_CHAT_LINES          equ  9
MAX_CHAT_LINE_LENGTH    equ 25
MAX_TEAM_NAME_LEN       equ 17
GAME_NAME_LENGTH        equ NICKNAME_LEN + 10

struc MP_Options
    .size:          resb 2
    .gameLength:    resb 2
    .pitchType:     resb 2
    .numSubs:       resb 1
    .maxSubs:       resb 1
    .skipFrames:    resb 1
    .padding:       resb 3
endstruc

struc LobbyState
    .numPlayers:                resb 4
    .MP_Options:
        .options.size:          resb 2
        .options.gameLength:    resb 2
        .options.pitchType:     resb 2
        .options.numSubs:       resb 1
        .options.maxSubs:       resb 1
        .options.skipFrames:    resb 1
        .options.padding:       resb 3
    .playerNames:               resb 4 * MAX_PLAYERS
    .playerFlags:               resb 4 * MAX_PLAYERS    ; bit 0 - is watcher, bit 1 - is ready
    .playerTeamsNames:          resb 4 * MAX_PLAYERS
    .chatLines:                 resb 4 * MAX_CHAT_LINES
    .chatLineColors:            resb 1 * MAX_CHAT_LINES
endstruc