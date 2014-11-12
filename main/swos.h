#pragma once

#include "types.h"

enum debugMessageClasses {
    LM_GAME_LOOP        = 1,
    LM_GAME_LOOP_STATE  = 2,
    LM_SYNC             = 4,
    LM_WATCHER          = 8,
    LM_SUBS_MENU        = 16,
};

/* screen width and height */
#define WIDTH  320
#define HEIGHT 200

/* screen width during the game */
#define GAME_WIDTH 384

#define TEAM_SIZE       684
#define TACTIC_SIZE     370

#define KEY_BUFFER_SIZE 10

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#pragma pack(push, 1) /* must pack them for file and SWOS memory access */

/* sprite structure - from dat files */
typedef struct SpriteGraphics {
    char  *sprData;      /* ptr to actual graphics                           */
    char  unk1[6];       /* unknown                                          */
    short width;         /* width                                            */
    short nlines;        /* height                                           */
    short wquads;        /* (number of bytes / 8) in one line                */
    short centerX;       /* center x coordinate                              */
    short centerY;       /* center y coordinate                              */
    uchar unk2;          /* unknown                                          */
    uchar nlines_div4;   /* nlines / 4                                       */
    short ordinal;       /* ordinal number in sprite.dat                     */
} SpriteGraphics;

/* sprite structure used during the game */
typedef struct Sprite {
    word teamNumber;     /* 1 or 2 for player controls, 0 for CPU            */
    word shirtNumber;    /* 1-11 for players, 0 for other sprites            */
    word frameOffset;
    void *animTablePtr;
    word startingDirection;
    byte playerState;
    byte playerDownTimer;
    word unk001;
    word unk002;
    void *frameIndicesTable;
    word frameIndex;
    word frameDelay;
    word cycleFramesTimer;
    word delayedFrameTimer;
    dword x;            /* fixed point, 16.16, signed, whole part high word  */
    dword y;
    dword z;
    word direction;
    word speed;
    dword deltaX;
    dword deltaY;
    dword deltaZ;
    word destX;
    word destY;
    byte unk003[6];
    word visible;       /* skip it when rendering if false                   */
    word pictureIndex;  /* -1 if none                                        */
    word saveSprite;
    dword ballDistance;
    word unk004;
    word unk005;
    word fullDirection;
    word beenDrawn;
    word unk006;
    word unk007;
    word unk008;
    word playerDirection;
    word isMoving;
    word tackleState;
    word unk009;
    word unk010;
    short cards;
    short injuryLevel;
    word tacklingTimer;
    word sentAway;
} Sprite;

typedef struct MenuEntry {
    word drawn;
    word ordinal;
    word invisible;
    word disabled;
    byte leftEntry;
    byte rightEntry;
    byte upEntry;
    byte downEntry;
    byte leftDirection;
    byte rightDirection;
    byte upDirection;
    byte downDirection;
    byte leftEntryDis;
    byte rightEntryDis;
    byte upEntryDis;
    byte downEntryDis;
    word x;
    word y;
    word width;
    word height;
    word type1;
    union {
        void (*EntryFunc)(word, word);
        word entryColor;
        word spriteIndex;
    } u1;
    word type2;
    word text_color;
    union {
        void (*EntryFunc2)(word, word);
        char *string;
        word spriteIndex;
        void *stringTable;
        void *multiLineText;
        word number;
        void *spriteCopy;
    } u2;
    void (*OnSelect)();
    word controlMask;
    void (*BeforeDraw)();
    void (*AfterDraw)();
} MenuEntry;

typedef struct TeamFile {
    byte teamFileNo;
    byte teamOrdinal;
    word globalTeamNumber;
    byte teamStatus;
    char teamName[17];
    byte unk1;
    byte unk2;
    byte tactics;
    byte league;
    byte prShirtType;
    byte prStripesColor;
    byte prBasicColor;
    byte prShortsColor;
    byte prSocksColor;
    byte secShirtTpe;
    byte secStripesColor;
    byte secBasicColor;
    byte secShortsColor;
    byte secSocksColor;
    char coachName[23];
    byte unk3;
    byte playerNumbers[16];
} TeamFile;


typedef struct TeamStatsData {
    word ballPossession;
    word cornersWon;
    word foulsConceded;
    word bookings;
    word sendingsOff;
    word goalAttempts;
    word onTarget;
} TeamStatsData;

typedef struct PlayerGame {
    byte substituted;
    byte index;
    byte goalsScored;
    byte shirtNumber;
    signed char position;
    byte face;
    byte isInjured;
    byte field_7;
    byte field_8;
    byte field_9;
    byte cards;
    byte field_B;
    char shortName[15];
    byte passing;
    byte shooting;
    byte heading;
    byte tackling;
    byte ballControl;
    byte speed;
    byte finishing;
    byte goalieDirection;
    byte injuriesBitfield;
    byte hasPlayed;
    byte face2;
    char fullName[23];
} PlayerGame;

typedef struct TeamGame {
    word prShirtType;
    word prShirtCol;
    word prStripesCol;
    word prShortsCol;
    word prSocksCol;
    word secShirtType;
    word secShirtCol;
    word secStripesCol;
    word secShortsCol;
    word secSocksCol;
    short markedPlayer;
    char teamName[17];
    byte unk_1;
    byte numOwnGoals;
    byte unk_2;
    PlayerGame players[16];
    byte unknownTail[686];
} TeamGame;

typedef struct TeamGeneralInfo {
    struct TeamGeneralInfo *opponentsTeam;
    word playerNumber;
    word plCoachNum;
    word isPlCoach;
    TeamGame *inGameTeamPtr;
    TeamStatsData *teamStatsPtr;
    word teamNumber;
    Sprite *(*players)[11];
    void *someTablePtr;
    word tactics;
    word tensTimer;
    Sprite *controlledPlayerSprite;
    Sprite *passToPlayerPtr;
    word playerHasBall;
    word playerHadBall;
    word currentAllowedDirection;
    word direction;
    byte quickFire;
    byte normalFire;
    byte joyIsFiring;
    byte joyTriggered;
    word header;
    word fireCounter;
    word allowedPlDirection;
    word shooting;
    byte ofs60;
    byte plVeryCloseToBall;
    byte plCloseToBall;
    byte plNotFarFromBall;
    byte ballLessEqual4;
    byte ball4To8;
    byte ball8To12;
    byte ball12To17;
    byte ballAbove17;
    byte prevPlVeryCloseToBall;
    word ofs70;
    Sprite *lastHeadingPlayer;
    word goalkeeperSavedCommentTimer;
    word ofs78;
    word goalkeeperJumpingRight;
    word goalkeeperJumpingLeft;
    word ballOutOfPlayOrKeeper;
    word goaliePlayingOrOut;
    word passingBall;
    word passingToPlayer;
    word playerSwitchTimer;
    word ballInPlay;
    word ballOutOfPlay;
    word ballX;
    word ballY;
    word passKickTimer;
    Sprite *passingKickingPlayer;
    word ofs108;
    word ballCanBeControlled;
    word ballControllingPlayerDirection;
    word ofs114;
    word ofs116;
    word spinTimer;
    word leftSpin;
    word rightSpin;
    word longPass;
    word longSpinPass;
    word passInProgress;
    word AITimer;
    word ofs134;
    word ofs136;
    word ofs138;    // timer
    word unkTimer;
    word goalkeeperPlaying;
    word resetControls;
    byte joy1SecondaryFire;
} TeamGeneralInfo;

typedef struct GoalInfo {
    word type;
    dword time;
} GoalInfo;

typedef struct SWOS_ScorerInfo {
    word shirtNum;
    word numSpritesTaken;
    word numGoals;
    GoalInfo goals[10];
} SWOS_ScorerInfo;

/* Player destination quadrants for every possible ball position in quadrants */
typedef struct PlayerPositions {
    /* hi nibble x, lo nibble y */
    byte positions[35];
} PlayerPositions;

typedef struct Tactics {
    char name[9];
    PlayerPositions playerPos[10];
    byte someTable[10];
    byte ballOutOfPlayTactics;
} Tactics;

#pragma pack(pop)

/* types of goals when printing game result */
enum GoalTypes {
    GOAL_TYPE_REGULAR,
    GOAL_TYPE_PENALTY,
    GOAL_TYPE_OWNGOAL
};

enum gameStates {
    ST_PLAYERS_TO_INITIAL_POSITIONS =   0,
    ST_GOAL_OUT_LEFT                =   1,
    ST_GOAL_OUT_RIGHT               =   2,
    ST_KEEPER_HOLDS_BALL            =   3,
    ST_CORNER_LEFT                  =   4,
    ST_CORNER_RIGHT                 =   5,
    ST_FREE_KICK_LEFT1              =   6,
    ST_FREE_KICK_LEFT2              =   7,
    ST_FREE_KICK_LEFT3              =   8,
    ST_FREE_KICK_CENTER             =   9,
    ST_FREE_KICK_RIGHT1             =  10,
    ST_FREE_KICK_RIGHT2             =  11,
    ST_FREE_KICK_RIGHT3             =  12,
    ST_FOUL                         =  13,
    ST_PENALTY                      =  14,
    ST_THROW_IN_FORWARD_RIGHT       =  15,
    ST_THROW_IN_CENTER_RIGHT        =  16,
    ST_THROW_IN_BACK_RIGHT          =  17,
    ST_THROW_IN_FORWARD_LEFT        =  18,
    ST_THROW_IN_CENTER_LEFT         =  19,
    ST_THROW_IN_BACK_LEFT           =  20,
    ST_STARTING_GAME                =  21,
    ST_CAMERA_GOING_TO_SHOWERS      =  22,
    ST_GOING_TO_HALFTIME            =  23,
    ST_PLAYERS_GOING_TO_SHOWER      =  24,
    ST_RESULT_ON_HALFTIME           =  25,
    ST_RESULT_AFTER_THE_GAME        =  26,
    ST_FIRST_EXTRA_STARTING         =  27,
    ST_FIRST_EXTRA_ENDED            =  28,
    ST_FIRST_HALF_ENDED             =  29,
    ST_GAME_ENDED                   =  30,
    ST_PENALTIES                    =  31,
    ST_GAME_IN_PROGRESS             = 100
};

enum Sprites {
    SPR_ADVERT_1_PART_1             =  227,
    SPR_ADVERT_2_PART_1             =  235,
    SPR_ADVERT_3_PART_1             =  243,
    SPR_SQUARE_GRID_FOR_RESULT      =  252,
    SPR_TEAM1_SCORERS               =  307,
    SPR_TEAM2_SCORERS               =  308,
    SPR_8_MINS                      =  328,
    SPR_88_MINS                     =  329,
    SPR_118_MINS                    =  330,
    SPR_TIME_BIG_0                  =  331,
    SPR_TIME_BIG_1                  =  332,
    SPR_TIME_BIG_2                  =  333,
    SPR_TIME_BIG_3                  =  334,
    SPR_TIME_BIG_4                  =  335,
    SPR_TIME_BIG_5                  =  336,
    SPR_TIME_BIG_6                  =  337,
    SPR_TIME_BIG_7                  =  338,
    SPR_TIME_BIG_8                  =  339,
    SPR_TIME_BIG_9                  =  340,
    SPR_BIG_S_FRAME_00              = 1241,
    SPR_BIG_S_FRAME_31              = 1272,
    SPR_COACH1_SITTING_1            = 1296,
    SPR_TEAM_2_RESERVE_6            = 1333,
    SPR_MAX                         = 1334
};

enum TacticsEnum {
    TACTICS_DEFAULT  = 0,
    TACTICS_5_4_1    = 1,
    TACTICS_4_5_1    = 2,
    TACTICS_5_3_2    = 3,
    TACTICS_3_5_2    = 4,
    TACTICS_4_3_3    = 5,
    TACTICS_4_2_4    = 6,
    TACTICS_3_4_3    = 7,
    TACTICS_SWEEP    = 8,
    TACTICS_5_2_3    = 9,
    TACTICS_ATTACK   = 10,
    TACTICS_DEFEND   = 11,
    TACTICS_USER_A   = 12,
    TACTICS_USER_B   = 13,
    TACTICS_USER_C   = 14,
    TACTICS_USER_D   = 15,
    TACTICS_USER_E   = 16,
    TACTICS_USER_F   = 17,
    TACTICS_MAX,
    TACTICS_IMPORTED = 1000
};

int __cdecl sprintf(char *buf, const char *ftm, ...);

#ifdef DEBUG
void FlushLogFile();
void EndLogFile();
void __cdecl WriteToLogFunc(const char *fmt, ...);
void __cdecl WriteToLogFuncM(dword messageClass, const char *fmt, ...);
void __cdecl WriteToLogFuncNoStamp(const char *str, ...);
#define WriteToLog(a) WriteToLogFunc a
#define WriteToLogM(a) WriteToLogFuncM a
#define WriteToLogNoStamp(a) WriteToLogFuncNoStamp a
void HexDumpToLog(const void *addr, int length, const char *title);
void HexDumpToLogM(dword messageClass, const void *addr, int length, const char *title);
#else
#define EndLogFile()                ((void)0)
#define FlushLogFile()              ((void)0)
#define WriteToLog(a)               ((void)0)
#define WriteToLogM(a)              ((void)0)
#define WriteToLogNoStamp(a)        ((void)0)
#define HexDumpToLog(a, b, c)       ((void)0)
#define HexDumpToLogM(a, b, c, d)   ((void)0)
#endif

/* declare symbols from SWOS */
#include "swossym.h"

#define MAX_SCORERS (sizeof(team1Scorers)/sizeof(team1Scorers[0]))

#define GetSprite(index) (spritesIndex[index])

/* this function is specific, as it takes one argument in x86 ebp register;
   only sucks that Watcom wouldn't allow ebp as parameter... */
void DrawSprite16Pixels(int saveSprite);
#pragma aux DrawSprite16Pixels =                \
    "mov  ebp, eax"                             \
    "mov  eax, offset SWOS_DrawSprite16Pixels"  \
    "call eax"                                  \
    parm [eax]                                  \
    modify [eax ebx ecx edx esi edi ebp];

extern void DrawBitmap(int x, int y, int width, int height, const char *data);
#pragma aux DrawBitmap parm [eax] [edi] [ecx] [ebx] [esi];
extern void SwitchToPrevVideoMode();
#pragma aux SwitchToPrevVideoMode "*";
extern void FatalError(char *msg);
extern void EndProgram(bool abnormalExit);
#pragma aux EndProgram "*" aborts;


/* in printstr.c */
void PrintSmallNumber(int num, int x, int y, bool inGame);

/* use strictly these functions for interfacing with SWOS code */
void calla(void *p);
#pragma aux calla = \
    "call eax"  \
    parm [eax]  \
    modify [eax ebx ecx edx esi edi ebp];

/* this version avoid pushing all the registers, in case we need it in SWOS code */
void calln(void *p);
#pragma aux calln = \
    "call eax"  \
    parm [eax]  \
    modify [];

/** It's so annoying, it's not accepting this function as no return.
    So better comment it out until that is sorted, or we have a time bomb. */
/*#pragma aux jmpa aborts;
void jmpa(void *p);
#pragma aux jmpa = \
    "push eax"  \
    "retn"      \
    parm [eax]  \
    modify [];*/

/* Damn it, Watcom. I told you ebp will be modified. */
void calla_save_ebp(void *p);
#pragma aux calla_save_ebp = \
    "push ebp"  \
    "call eax"  \
    "pop  ebp"  \
    parm [eax]  \
    modify [eax ebx ecx edx esi edi];

/* Use this macro to replace function call with our own. */
#define PatchCall(where, ofs, func) (*(dword *)((char *)(where) + (dword)(ofs)) = \
    (dword)(func) - (dword)(where) - (dword)(ofs) + 1 - 5)
#define PatchDword(where, ofs, value) (*(dword *)((char *)(where) + (dword)(ofs)) = (dword)(value))
#define PatchWord(where, ofs, value) (*(word *)((char *)(where) + (dword)(ofs)) = (word)(value))
#define PatchByte(where, ofs, value) (*(byte *)((char *)(where) + (dword)(ofs)) = (byte)(value))

/** Use this macro to patch in a function call directly in code, when it's not feasible to patch
    call instructions themselves (i.e. function is call from many places). It will create folowing
    machine code (calla equivalent):

    mov  eax, offset hookFn  ; b8 xx xx xx xx
    call eax                 ; ff d0

    Takes up 7 bytes.
*/
#define PatchHook(loc, hookFn)                      \
    *(char *)loc = 0xb8;                            \
    *(dword *)((char *)loc + 1) = (dword)hookFn;    \
    *(word *)((char *)loc + 5) = 0xd0ff;

/* Version that will patch and pad with nops. */
#define PatchHookAndPad(loc, hookFn, padding)   \
    PatchHook(loc, hookFn);                     \
    {                                           \
        int i;                                  \
        for(i = 0; i < padding; i++)            \
            ((char *)loc)[i + 7] = 0x90;        \
    }

/* this one needs special attention, as it destroys ebp */
int Text2Sprite(int x, int y, int pictureIndex, const char *text, const void *charsTable);

#pragma aux FatalError "*" parm [edx] aborts;