#include <stdlib.h>
#include <string.h>
#include <i86.h>
#include <time.h>
#include <assert.h>
#include "types.h"
#include "options.h"
#include "mplayer.h"
#include "xmltree.h"

word chairmanScenes;
word commentary;
word pitchType;
word allPlayerTeamsEqual;
word autoSaveHighlights;
word menuMusic;
word autoReplays;
word gameLength;

#pragma aux chairmanScenes "*";
#pragma aux commentary "*";
#pragma aux pitchType "*";
#pragma aux allPlayerTeamsEqual "*";
#pragma aux autoSaveHighlights "*";
#pragma aux menuMusic "*";
#pragma aux autoReplays "*";
#pragma aux gameLength "*";

static char *strncpy_stub(char *dst, const char *src, size_t n)
{
    assert(!n || dst && src);
    while (n-- && (*dst++ = *src++));
    dst -= !n;
    return dst;
}

char *swos_libc_strncpy(char *dst, const char *src, int n)
{
    return strncpy_stub(dst, src, n);
}

void *swos_libc_memset(void *ptr, int value, int num)
{
    assert(!num || ptr);
    return memset(ptr, value, num);
}

int swos_libc_strcmp(const char *str1, const char *str2)
{
    assert(str1 && str2);
    return strcmp(str1, str2);
}

int swos_libc_strncmp(const char *str1, const char *str2, size_t n)
{
    assert(str1 && str2 && (int)n >= 0);
    return strncmp(str1, str2, n);
}

int swos_libc_strlen(const char *str)
{
    assert(str);
    return strlen(str);
}

void swos_libc_segread(struct SREGS *sregs)
{
    assert(sregs);
    segread(sregs);
}

int swos_libc_int386x(int vec, union REGS *in, union REGS *out, struct SREGS *sregs)
{
    assert(in && out && sregs);
    return int386x(vec, in, out, sregs);
}

void swos_libc_exit(int status)
{
    exit(status);
}

void *swos_libc_memmove(void *dst, const void *src, int n)
{
    assert(!n || dst && src);
    return memmove(dst, src, n);
}

time_t swos_libc_time(time_t *timer)
{
    return time(timer);
}

char *swos_libc_ctime(const time_t *timer)
{
    return ctime(timer);
}

static MP_Options mpOptions = { sizeof(MP_Options), 1, 2, 3, 5, 1, 8, 'z', 'k' };

/* Return currently active MP options for options manager. */
MP_Options *getMPOptions()
{
    return &mpOptions;
}


/* Update this whenever MP_Options get changed! */
static void registerMPOptions(RegisterOptionsFunc registerOptions)
{
    assert(sizeof(MP_Options) == 12);
    registerOptions("game", 4, "Default settings for multiplayer match", 38,
        "%n%c" "%2d/version" "%2d/gameLength" "%2d/pitchType" "%1d/numSubs" "%1d/maxSubs"
        "%1d/skipFrames" "%1d/networkTimeout", getMPOptions, sizeof(MP_Options));
}

char playerNick[NICKNAME_LEN + 1] = "SCHMIDT";  /* player name for online games         */
char gameName[GAME_NAME_LENGTH] = "ADDAGIO";    /* name of current game                 */
dword currentTeamId = -1;                       /* id of current team                   */

void RegisterNetworkOptions(RegisterOptionsFunc registerOptions)
{
    registerOptions("multiplayer", 11, "Options for multiplayer games", 29,
        "%*s/playerNick%*s/gameName%4d/team", sizeof(playerNick), playerNick,
        sizeof(gameName), gameName, &currentTeamId);
    registerMPOptions(registerOptions);
}

static byte pl2Keyboard;
static byte codeUp;
static byte codeDown;
static byte codeLeft;
static byte codeRight;
static byte codeFire1;
static byte codeFire2;

void RegisterControlsOptions(RegisterOptionsFunc registerOptions)
{
    registerOptions("controls", 8, "Player 2 keyboard controls", 26,
        "%1d/pl2Keyboard" "%1d/codeUp" "%1d/codeDown"
        "%1d/codeLeft" "%1d/codeRight" "%1d/codeFire1" "%1d/codeFire2", &pl2Keyboard,
        &codeUp, &codeDown, &codeLeft, &codeRight, &codeFire1, &codeFire2);
}

static Tactics mpTactics[6];

void RegisterUserTactics(RegisterOptionsFunc registerOptions)
{
    registerOptions("tactics", 7, "User tactics in multiplayer games", 33, "%n"
        "%370b/USER_A" "%370b/USER_B" "%370b/USER_C"
        "%370b/USER_D" "%370b/USER_E" "%370b/USER_F",
        &mpTactics[0], &mpTactics[1], &mpTactics[2], &mpTactics[3], &mpTactics[4], &mpTactics[5]);
}

bool ValidateUserMpTactics()
{
    return true;
}