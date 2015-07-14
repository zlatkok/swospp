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

union FixedPoint {
    struct {
        word wholePart;
        word fraction;
    };
    dword data;
    FixedPoint& operator=(dword num) {
        data = num;
        return *this;
    }
    operator int() const { return static_cast<int>(data); }
};

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
    FixedPoint x;       /* fixed point, 16.16, signed, whole part high word  */
    FixedPoint y;
    FixedPoint z;
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


typedef struct Menu {
    void (*onInit)();
    void (*afterDraw)();
    void (*onDraw)();
    MenuEntry *currentEntry;
    word numEntries;
    char *endOfMenuPtr;
} Menu;


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

#ifdef __GNUC__
#ifndef __cdecl
#define __cdecl __attribute__((cdecl))
#endif
#endif

int __cdecl sprintf(char *buf, const char *ftm, ...);
#ifdef DEBUG
/* we wanna overload these */
void __cdecl WriteToLogFunc(dword messageClass, const char *fmt, ...);
void HexDumpToLog(const void *addr, int length, const char *title = "memory block");
void HexDumpToLog(dword messageClass, const void *addr, int length, const char *title);
#endif

extern "C" {

#ifdef DEBUG
void FlushLogFile() asm ("FlushLogFile");
void EndLogFile() asm ("EndLogFile");
void __cdecl WriteToLogFunc(const char *fmt, ...);
void __cdecl WriteToLogFuncNoStamp(const char *str, ...);
#define WriteToLog(...) WriteToLogFunc(__VA_ARGS__)
#define WriteToLogNoStamp(a) WriteToLogFuncNoStamp a
#else
#define EndLogFile()                ((void)0)
#define FlushLogFile()              ((void)0)
#define WriteToLog(a, ...)          ((void)0)
#define WriteToLogNoStamp(a)        ((void)0)
#define HexDumpToLog(a, ...)        ((void)0)
#endif

/* declare symbols from SWOS */
#include "swossym.h"

#define MAX_SCORERS (sizeof(team1Scorers)/sizeof(team1Scorers[0]))

#define GetSprite(index) (spritesIndex[index])

/* this function is specific, as it takes one argument in x86 ebp register;
   only sucks that Watcom and GCC wouldn't allow ebp as parameter... */
static inline __attribute__((always_inline)) void DrawSprite16Pixels(int saveSprite)
{
    asm volatile (
        "push ebp                   \n"
        "mov  ebp, %[saveSprite]    \n"
        "call %[addr]               \n"
        "pop  ebp                   \n"
        :
        : [addr] "rm" (SWOS_DrawSprite16Pixels), [saveSprite] "rm" (saveSprite)
        : "cc", "memory", "eax", "ebx", "esi", "edi"
    );
}


extern void DrawBitmap(int x, int y, int width, int height, const char *data);
extern void SwitchToPrevVideoMode();
extern void FatalError(const char *msg) __attribute__((noreturn));
extern void EndProgram(bool32 abnormalExit) __attribute__((noreturn));


/* in printstr.c */
void PrintSmallNumber(int num, int x, int y, bool32 inGame);

/* use strictly these functions for interfacing with SWOS code */
static inline __attribute__((always_inline)) void calla(void *addr)
{
    /** There were problems relaying to GCC that this function should clobber all registers including the one
        used to do the call, but not ebp. If we specify that 7 general registers were used ("r"), it would work
        in case function does not have stack frame, even if ebp was wrongly assumed to be clobbered. However
        once the stack frame was added ebp became unavailable for use and it caused impossible constraints error.
        In case we reserved only 6 registers, and function did not use stack frame it would be possible that ebp
        ended up as used register, and GCC might think that register that actually got used inside the function
        did not change. Even explicitly listing registers ("=&abcdSD") was causing impossible constraints error.
        Final incarnation bounds address to eax register, since all registers are assumed to be used anyway, and
        seems to avoid previous pitfalls. */
    asm volatile (
        "push ebp       \n"
        "call %[addr]   \n"
        "pop  ebp       \n"
        : [addr] "+a" (addr)
        :
        : "ebx", "ecx", "edx", "esi", "edi", "cc", "memory"
    );
}

/* this version avoids pushing all the registers, in case we need it in SWOS code */
static inline __attribute__((always_inline)) void calln(void *addr)
{
    asm volatile (
        "call %[addr]"
        : [addr] "+r" (addr)
        :
        : "cc", "memory"
    );
}

//untested!
static inline __attribute__((always_inline)) void jmpa(void *addr)
{
    asm volatile (
        "jmp  %[addr]"
        : [addr] "+r,m" (addr)
        :
    );
}

/* Modifications on ebp must be handled with special care. */
static inline __attribute__((always_inline)) void calla_ebp_safe(void *addr)
{
    asm volatile (
        "call %[addr]"
        : [addr] "+a" (addr)
        :
        : "ebx", "ecx", "edx", "esi", "edi", "cc", "memory"
    );
}


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
#define PatchHook(loc, hookFn)                          \
    {                                                   \
        *(char *)loc = 0xb8;                            \
        *(dword *)((char *)loc + 1) = (dword)hookFn;    \
        *(word *)((char *)loc + 5) = 0xd0ff;            \
    }

/* Version that will patch and pad with nops. */
#define PatchHookAndPad(loc, hookFn, padding)   \
    {                                           \
        PatchHook(loc, hookFn);                 \
        for(int i = 0; i < padding; i++)        \
            ((char *)loc)[i + 7] = 0x90;        \
    }

/* this one needs special attention, as it destroys ebp */
int Text2Sprite(int x, int y, int pictureIndex, const char *text, const void *charsTable);

}
