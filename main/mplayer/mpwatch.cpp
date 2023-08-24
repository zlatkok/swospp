/** mpwatch.cpp

    Watchers support implementation. Watcher packets will not be synchronized in any way,
    watchers will simply show latest packet they received each frame.
*/

#include <limits.h>
#include "qalloc.h"
#include "mpwatch.h"
#include "mplayer.h"
#include "mppacket.h"

#define MAX_SPRITES                 79
#define WATCHER_PACKETS_BUFFERED    32

/* arrow, (coach + bench players) x 2 */
#define BENCH_MAX_SPRITES   1 + 6 + 6

#pragma pack(push, 1)
struct ScorerInfo {
    byte indexNumGoals;     /* packed, hi nibble player index, lo nibble number of goals */
    dword goalTypes;        /* packed, 2 bits per goal, 0 = regular, 1 = penatly, 2 = own goal, variable size */
    dword goalTime[10];     /* ascii digits of time, always 3 digits */
};

/* Structure of watcher packet that will be sent from players to watchers. When sending only send
   actual number of sprites, scorers and goals - maximum is just for storing packets. */
struct WatcherPacket {
    word type;                  /* PT_GAME_SPRITES */
    byte numSprites;
    byte rendered;
    dword cameraX;
    dword cameraY;
    int frameNo;
    dword gameTime;             /* number of minutes, three digits, one digit per byte */
    word stoppageTimer;         /* needed for animated patterns */
    word adOffsets[3];          /* support for scrolling advertisements */
    byte numScorers;            /* packed, 4 bits for each team, team1 low nibble, team2 high */
    byte flags;                 /* showing animated patterns state in low nibble, flags in high */
    char sig[2];                /* "zk" :P, more like padding */
    dword sprites[MAX_SPRITES]; /* bit-packed 10/10/11/1 - x/y/picture index/save flag, numSprites is length */
    ScorerInfo scorerInfo[2][MAX_SCORERS];  /* scorers for both team */
};

struct BenchMenuPlayer {
    word shirtNumber:8;
    word face:2;
    word position:3;
    word isMarked:1;
    word isInjured:1;
    schar cards:2;
    uchar injury:3;
};

struct BenchInfo {
    byte showingSubsMenu:1;
    byte showingFormationMenu:1;
    byte benchTeam:1;
};

struct SubsMenuData {
    BenchMenuPlayer players[2][11];
    short plToBeSubstitutedPos:5;
    short plToBeSubstitutedOrd:5;
    short selectedPlayerInSubs:5;
    word plToEnterGameIndex:4;
    word enqueueSample:1;
    short subsState:4;
    word frameCount;
};

struct FormationMenuData {
    uint selectedFormationEntry:5;
    uint enqueueSample:1;
};

#pragma pack(pop)

/* Return number of bytes needed to hold goalTypes based on numGoals and assumption of 2 bits per goal type. */
static __inline int getGoalTypesSize(int numGoals)
{
    if (numGoals <= 4)
        return 1;
    else if (numGoals <= 8)
        return 2;
    else
        return 4;
}

#define SHOWING_STATS_FLAG          1 << 4
#define AUTO_SHOWING_STATS_FLAG     1 << 5
#define END_GAME_FLAG               1 << 6
#define SHOWING_BENCH_FLAG          1 << 7

static WatcherPacket *m_watcherPackets; /* depending if we're watching or not, memory for 1 or more packets */
static int m_lastWatcherFrame;
static word m_lastWatcherTick;          /* last tick we received a watcher packet */
static int m_benchSpritesIndex asm("m_benchSpritesIndex") __attribute__((used));
static dword m_benchSpritesStack[BENCH_MAX_SPRITES] asm("m_benchSpritesStack") __attribute__((used));   /* basic sprites when drawing bench */

static struct HookFlags {
    byte showingSubsMenu:1;
    byte showingFormationMenu:1;
    byte enqueueSubstituteSample:1;
    byte enqueueTacticsChangedSample:1;
} hookFlags;

static int PackBenchData(BenchInfo *benchInfo);
static void StoreWatcherPacket(const WatcherPacket *packet);
static void ApplyWatcherPacket(const WatcherPacket *packet);
static int GetIndex(const TeamGame *team, int shirtNumber);
static dword PackSprite(dword spriteIndex, int x, int y, bool saveSprite) asm("PackSprite") __attribute__((used));
static byte *PackScorers(const SWOS_ScorerInfo *scorers, byte *destScorers, byte *numScorers, int teamNumber);
static WatcherPacket *GetWatcherPacketsData(int *emptySpotIndex, int *numPackets, int frameNo);
static bool VerifyPacket(const WatcherPacket *wp, int length);
static void PackAdData(WatcherPacket *wp);
/* referenced from asm */
void __attribute__((used)) DrawSpriteHook() asm("DrawSpriteHook");
static void DrawSubstitutesMenuHook();
static void DrawFormationMenuHook();
static void EnqueueSubstituteSampleHook();

/** InitWatchers

    weAreWatching - informs if we are player or watcher

    Allocate memory to support watchers, only one packet if we're playing, or whole bunch
    if we're watching, to buffer up. Hook bench drawing to capture those sprites for transfer too.
    Hook drawing of subs menu to signal we need to capture and send that data too.
*/
void InitWatchers(bool weAreWatching)
{
    m_lastWatcherTick = g_currentTick;
    m_lastWatcherFrame = 0;
    m_benchSpritesIndex = -1;

    if (weAreWatching) {
        m_watcherPackets = (WatcherPacket *)qAlloc(WATCHER_PACKETS_BUFFERED * sizeof(WatcherPacket));
        memset(m_watcherPackets + sizeof(WatcherPacket), -1, (WATCHER_PACKETS_BUFFERED - 1) * sizeof(WatcherPacket));
        subsMenuX = 100;    /* just a constant in a need of initialization */
        formationMenuX = 100;
    } else {
        m_watcherPackets = (WatcherPacket *)qAlloc(sizeof(WatcherPacket) + BENCH_MAX_SPRITES * sizeof(dword));
        PatchCall(DrawBenchAndSubsMenu,     0x170, DrawSpriteHook);
        PatchCall(DrawBenchPlayersAndCoach, 0x15d, DrawSpriteHook);
        PatchCall(DrawBenchPlayersAndCoach, 0x393, DrawSpriteHook);
        PatchCall(DrawBenchAndSubsMenu,     0x1d8, DrawSubstitutesMenuHook);
        PatchCall(DrawBenchAndSubsMenu,     0x1e6, DrawSubstitutesMenuHook);
        PatchCall(DrawBenchAndSubsMenu,     0x202, DrawSubstitutesMenuHook);
        PatchCall(DrawBenchAndSubsMenu,     0x1f4, DrawFormationMenuHook);
        PatchCall(SubstitutePlayerIfFiring, 0x3e9, EnqueueSubstituteSampleHook);
    }

    WriteToLog(LM_WATCHER, "Watcher support initialized... m_watcherPackets = %#x", m_watcherPackets);
}

void CleanupWatchers()
{
    qFree(m_watcherPackets);
    m_watcherPackets = nullptr;
    PatchDword(DrawBenchAndSubsMenu,     0x170, 0xfffb1823);
    PatchDword(DrawBenchPlayersAndCoach, 0x15d, 0xfffb15af);
    PatchDword(DrawBenchPlayersAndCoach, 0x393, 0xfffb1379);
    PatchDword(DrawBenchAndSubsMenu,     0x1d8, 0x000004af);
    PatchDword(DrawBenchAndSubsMenu,     0x1e6, 0x000004a1);
    PatchDword(DrawBenchAndSubsMenu,     0x202, 0x00000485);
    PatchDword(DrawBenchAndSubsMenu,     0x1f4, 0x000013e8);
    PatchDword(SubstitutePlayerIfFiring, 0x3e9, 0xfffae294);
}

/** HandleWatcherPacket

    packet      -> points to packet data
    length      -  length of packet data
    currentTick -  last packet received time, apply if packet valid
    abort       -  will be set to true if players are informing us of game end

    Return true if given packet is a watcher packet. If it indeed is a watcher packet, process it.
*/
bool HandleWatcherPacket(const char *packet, int length, int currentTick, bool *abort)
{
    *abort = false;
    if (GetRequestType(packet, length) == PT_GAME_SPRITES) {
        /* if we just received a packet for watcher, store it into buffer */
        WatcherPacket *wp = (WatcherPacket *)packet;

        /* end game packet has top priority */
        if (length >= (int)offsetof(WatcherPacket, sprites) && (wp->flags & END_GAME_FLAG)) {
            *abort = true;
            return true;
        }

        HexDumpToLog(LM_WATCHER, packet, length, "watcher packet");
        if (!VerifyPacket(wp, length)) {
            WriteToLog("Watcher packet verification failed.");
            return false;
        }

        m_lastWatcherTick = currentTick;
        WriteToLog(LM_WATCHER, "Received watcher packet for frame %d, x = %d, y = %d, numSprites = %d",
            wp->frameNo, wp->cameraX >> 16, wp->cameraY >> 16, wp->numSprites);
        /* don't bother checking, just overwrite packet even if we already have it */
        StoreWatcherPacket(wp);
        return true;
    }

    return false;
}

/** WatcherTimeout

    Return true if we haven't received a watcher packet for a while.
*/
bool WatcherTimeout(word networkTimeout)
{
    return m_lastWatcherTick + networkTimeout < g_currentTick;
}

/** CreateWatcherPacket

    watcherPacketSize -> will receive size of the created watcher packet
    currentFrameNo    -  current frame we're in

    Creates a packet for watcher from our current state that is ready to be sent, and returns a pointer to it.
    Returned pointer should not be freed. Anything we send to the watchers goes from here.
*/
char *CreateWatcherPacket(int *watcherPacketSize, int currentFrameNo)
{
    int i, numSprites = 0, variablePartSize = 0;
    dword *destSprites = m_watcherPackets->sprites;
    byte *scorerInfo, *scorersStart;

    assert(m_watcherPackets && numSpritesToRender <= MAX_SPRITES);
    /* skip all this if we weren't even initialized (ie. no watchers) */

    if (!m_watcherPackets)
        return nullptr;

    m_lastWatcherTick = g_currentTick;  /* reset timer to avoid timeout at start */
    WriteToLog(LM_WATCHER, "Creating watcher packet for frame %d...", currentFrameNo);
    m_watcherPackets->type = PT_GAME_SPRITES;
    m_watcherPackets->frameNo = currentFrameNo;
    memcpy(&m_watcherPackets->cameraX, &cameraXFraction, sizeof(dword));
    memcpy(&m_watcherPackets->cameraY, &cameraYFraction, sizeof(dword));
    m_watcherPackets->gameTime = gameTime;

    for (i = 0; i < numSpritesToRender; i++) {
        /* we'll pack everything into 32-bit int - a perfect fit :P */
        Sprite *s = sortedSprites[i];
        if ((short)s->pictureIndex >= 0) {
            /* note that the pointer will be invalid for custom player numbers, but don't access it in that case */
            SpriteGraphics *sg = spritesIndex[s->pictureIndex];

            /* must send transformed coordinates, and don't forget to preserve sign */
            int x = s->x - cameraX;
            int y = s->y - cameraY - s->z;
            assert(s->pictureIndex < 1334 || (s->pictureIndex >= 9187 && s->pictureIndex <= 9442));

            /* don't subtract center coordinates for custom draw player numbers */
            if (s->pictureIndex < SPR_MAX) {
                x -= sg->centerX;
                y -= sg->centerY;
            }

            /* skip out of screen sprites, but don't do that check for high player numbers */
            if (s->pictureIndex >= SPR_MAX || (x < 336 && y < 200 && x >= -sg->width && y >= -sg->nlines)) {
                *destSprites++ = PackSprite(s->pictureIndex, x, y, s->saveSprite);
                numSprites++;
            }
        }
    }

    m_watcherPackets->flags = 0;
    if (m_benchSpritesIndex >= 0 || hookFlags.showingSubsMenu || hookFlags.showingFormationMenu) {
        /* we are in bench mode */
        WriteToLog("Bench is active... We got %d bench sprites.", m_benchSpritesIndex + 1);
        m_watcherPackets->flags |= SHOWING_BENCH_FLAG;
        if (m_benchSpritesIndex >= 0) {
            /* send them as normal sprites */
            assert(m_benchSpritesIndex < (int)sizeofarray(m_benchSpritesStack));
            for (i = 0; i <= m_benchSpritesIndex; i++) {
                *destSprites++ = m_benchSpritesStack[i];
                numSprites++;
            }
        }
        variablePartSize = PackBenchData((BenchInfo *)destSprites);
    } else if (showingStats || (eventTimer && statsTimeout > 0)) {
        /* we will use space for scorers to send statistics */
        m_watcherPackets->flags |= showingStats ? SHOWING_STATS_FLAG : AUTO_SHOWING_STATS_FLAG;
        memcpy(m_watcherPackets->sprites + numSprites, &team1StatsData, 2 * sizeof(TeamStatsData));
        m_watcherPackets->numScorers = 0;
        variablePartSize = 2 * sizeof(TeamStatsData);
    } else {
        WriteToLog(LM_WATCHER, "Packing team1 scorers...");
        scorersStart = (byte *)&m_watcherPackets->sprites[numSprites];
        byte numScorers;
        scorerInfo = PackScorers(team1Scorers, scorersStart, &numScorers, 1);
        m_watcherPackets->numScorers = numScorers & 0x0f;
        assert(numScorers <= MAX_SCORERS);
        WriteToLog(LM_WATCHER, "Packing team2 scorers...");
        scorerInfo = PackScorers(team2Scorers, scorerInfo, &numScorers, 2);
        m_watcherPackets->numScorers |= numScorers << 4;
        assert(numScorers >> 4 <= (int)MAX_SCORERS);
        variablePartSize = scorerInfo - scorersStart;
    }

    PackAdData(m_watcherPackets);
    m_watcherPackets->rendered = false;
    m_watcherPackets->flags |= animPatternsState & 0x0f;
    m_watcherPackets->stoppageTimer = stoppageTimer;
    m_watcherPackets->numSprites = numSprites;
    m_watcherPackets->sig[0] = 'z'; m_watcherPackets->sig[1] = 'k';
    *watcherPacketSize = offsetof(WatcherPacket, sprites) + numSprites * sizeof(dword) + variablePartSize;
    WriteToLog(LM_WATCHER, "Num. sprites = %d, num scorers = %d, bench stack index = %d, "
        "showingSubsMenu = %d, showingFormationMenu = %d", numSprites,
        (m_watcherPackets->numScorers & 0x0f) + (m_watcherPackets->numScorers >> 4), m_benchSpritesIndex,
        hookFlags.showingSubsMenu, hookFlags.showingFormationMenu);

    //HexDumpToLog(LM_WATCHER, m_watcherPackets, *watcherPacketSize, "watcher packet to send");
    return (char *)m_watcherPackets;
}

/* Apply data from watcher packet to SWOS internal state. */
void ApplyWatcherPacket()
{
    int emptyPacketIndex, numPackets;
    WatcherPacket *packet = GetWatcherPacketsData(&emptyPacketIndex, &numPackets, -1);
    WriteToLog(LM_WATCHER, "Applying oldest watcher packet (num. packets = %d)...", numPackets);
    ApplyWatcherPacket(packet);
    packet->rendered = true;

    if (numPackets > 1) {
        WriteToLog(LM_WATCHER, "Removing watcher packet %d", packet->frameNo);
        packet->type = -1;  /* effectively removes it from the queue */
        m_lastWatcherFrame = packet->frameNo + 1;
    }
}

const char *GetEndGameWatcherPacket(int *length)
{
    assert(m_watcherPackets);
    m_watcherPackets->flags = END_GAME_FLAG;
    m_watcherPackets->numSprites = m_watcherPackets->numScorers = 0;
    m_watcherPackets->sig[0] = 'z'; m_watcherPackets->sig[1] = 'k';
    *length = offsetof(WatcherPacket, sprites);
    return (const char *)m_watcherPackets;
}

/*
    internal routines
    -----------------
*/

/** DrawSpriteHook

    Called in place of DrawSpriteInGame when drawing bench. Capture sprites drawn in bench mode,
    outside menus. Pack them and store into stack. Stack index will be used as a part of test if
    we are in bench mode. Sprites will be sent as ordinary sprites in a watcher packet. On entry:

    D0  - sprite index
    D1  - x
    D2  - y
    ebp - save sprite flag (-1 = save, not -1 = don't save)

    Written in assembly just to be safe with ebp.
*/
asm (
"DrawSpriteHook:     \n"
    "mov  edi, m_benchSpritesIndex      \n"     // range check, as we might be called with watchers not running
    "cmp  edi, " STRINGIFY(BENCH_MAX_SPRITES)   "\n"
    "jge  return                        \n"
    "mov  ecx, ebp                      \n"     // save sprite flag
    "mov  edx, D1                       \n"
    "mov  ebx, D2                       \n"
    "sub  dx, cameraX                   \n"     // don't forget to take in account camera and sprite centering
    "sub  bx, cameraY                   \n"
    "mov  eax, D0                       \n"
    "and  eax, 0xffff                   \n"     // SWOS is sending low word only
    "mov  esi, spritesIndex[eax * 4]    \n"
    "sub  dx, [esi + 16]                \n"     // centerX
    "sub  bx, [esi + 18]                \n"     // centerY
    "movsx ebx, bx                      \n"
    "movsx edx, dx                      \n"
    "call PackSprite                    \n"     // will not change ebp or edi
    "inc  edi                           \n"     // edi = m_benchSpritesIndex
    "mov  [m_benchSpritesStack + edi * 4], eax  \n"
    "mov  m_benchSpritesIndex, edi      \n"     // m_benchSpritesStack[++m_benchSpritesIndex] = packed sprite
"return:    \n"
    "push offset SWOS_DrawSpriteInGame  \n"     // resume normal execution, params already set
    "ret    \n"
);

static void DrawSubstitutesMenuHook()
{
    hookFlags.showingSubsMenu = true;
    calla(SWOS_DrawSubstitutesMenu);
}

static void DrawFormationMenuHook()
{
    hookFlags.showingFormationMenu = true;
    calla(SWOS_DrawFormationMenu);
}

/* Try capturing moment when substitute was made so watcher can play that sound too. */
static void EnqueueSubstituteSampleHook()
{
    hookFlags.enqueueSubstituteSample = true;
    calla(EnqueueSubstituteSample);
}

static dword PackSprite(dword spriteIndex, int x, int y, bool saveSprite)
{
    dword packedSprite = (x & 0x3ff) | ((y & 0x3ff) << 10);
    /* we don't have enough bits to transfer big shirt numbers as they are, so take special attention */
    if (spriteIndex >= 9000 + 1187)
        spriteIndex -= 9000 + 1187 - SPR_MAX;
    return packedSprite | ((saveSprite != 0) << 31) | ((spriteIndex & 0x7ff) << 20);
}

/** VerifyPacket

    wp     -> packet to verify
    length -  actual length of received packet

    Do some rudimentary checking and return true if packet seems ok. Most importantly, check the
    length so routines that process the packet later can be simplified, with overflow check removed.
    No mistake here please! ;)
*/
static bool VerifyPacket(const WatcherPacket *wp, int length)
{
    if (length < (int)offsetof(WatcherPacket, sprites))
        return false;

    if ((length -= offsetof(WatcherPacket, sprites) + wp->numSprites * sizeof(dword)) < 0)
        return false;

    if (wp->sig[0] != 'z' || wp->sig[1] != 'k')
        return false;

    if (wp->flags & (SHOWING_STATS_FLAG | AUTO_SHOWING_STATS_FLAG)) {
        /* in this case we only got 2 stats data structures, and that's all */
        if (length != 2 * sizeof(TeamStatsData))
            return false;
    } else if (wp->flags & SHOWING_BENCH_FLAG) {
        const BenchInfo *bi = (BenchInfo *)&wp->sprites[wp->numSprites];
        if ((length -= sizeof(BenchInfo)) < 0)
            return false;

        if (bi->showingSubsMenu && length != sizeof(SubsMenuData))
            return false;
        else if (bi->showingFormationMenu && length != sizeof(FormationMenuData))
            return false;

        return true;
    } else {
        if (wp->numScorers) {
            byte *scorers = (byte *)&wp->sprites[wp->numSprites];
            int i;

            /* go through both teams scorers */
            for (i = 0; i < (wp->numScorers & 0x0f) + (wp->numScorers >> 4); i++) {
                int numGoals, goalTypesSize, goalsLength;
                /* 1 byte for index/number of goals scored */
                if (length-- <= 0)
                    return false;

                /* goal types size is variable */
                goalTypesSize = getGoalTypesSize(numGoals = *scorers++ & 0x0f);

                /* assume at least one goal since we get the scorer */
                if ((length -= goalTypesSize) <= 0)
                    return false;

                if ((length -= (goalsLength = numGoals * sizeof(dword))) < 0)
                    return false;

                scorers += goalTypesSize + goalsLength;
            }
        }
    }

    return true;
}

/* starting sprites for scrolling advertisements */
static const byte adSpriteIndices[] = { SPR_ADVERT_1_PART_1, SPR_ADVERT_2_PART_1, SPR_ADVERT_3_PART_1 };
static byte **adDataPointers[] = { advert1Pointers, advert2Pointers, advert3Pointers };

static void PackAdData(WatcherPacket *wp)
{
    assert(sizeofarray(adSpriteIndices) == sizeofarray(adDataPointers));
    for (size_t i = 0; i < sizeofarray(adSpriteIndices); i++) {
        wp->adOffsets[i] = GetSprite(adSpriteIndices[i])->data - (char *)adDataPointers[i][0];
        WriteToLog(LM_WATCHER, "Ad %d offset = %d", i, wp->adOffsets[i]);
        assert((short)wp->adOffsets[i] >= 0);
    }
}

static void ApplyAdData(const WatcherPacket *wp)
{
    assert(sizeofarray(adSpriteIndices) == sizeofarray(adDataPointers));
    for (size_t i = 0; i < sizeofarray(adSpriteIndices); i++) {
        WriteToLog(LM_WATCHER, "ad %d offset = %d", i, wp->adOffsets[i]);
        for (size_t j = 0; j < sizeofarray(advert1Pointers); j++) {
            SpriteGraphics *spr = GetSprite(adSpriteIndices[i] + j);
            spr->data = (char *)adDataPointers[i][j] + wp->adOffsets[i];
            /* important - fix advert sprites height */
            spr->nlines = 6;
        }
    }
}

/** PackBenchData

    benchInfo -> bench info packet

    Structure:
    - fixed part: number of bench sprites, flags for showing subs and formation menu
    - subs menu data if menu is showing
    - formation menu data if menu is showing
*/
static int PackBenchData(BenchInfo *benchInfo)
{
    static const TeamGeneralInfo *teams[2] = { &leftTeamData, &rightTeamData };
    int packetLength = sizeof(BenchInfo);

    if (m_benchSpritesIndex < 0 && !hookFlags.showingSubsMenu && !hookFlags.showingFormationMenu)
        return 0;

    benchInfo->showingSubsMenu = hookFlags.showingSubsMenu;
    benchInfo->showingFormationMenu = hookFlags.showingFormationMenu;
    /* swap team to display in 2nd half */
    benchInfo->benchTeam = (benchTeam != &leftTeamData) ^ (halfNumber - 1);

    if (hookFlags.showingSubsMenu) {
        SubsMenuData *subs = (SubsMenuData *)&benchInfo[1];
        /* fill relevant info about teams */
        for (size_t i = 0; i < sizeofarray(teams); i++) {
            const PlayerGame *pl = (PlayerGame *)((char *)teams[i]->inGameTeamPtr + offsetof(TeamGame, players));
            /* swap sent teams on halftime since watcher won't be swapping them */
            BenchMenuPlayer *bPl = subs->players[i ^ (halfNumber - 1)];
            assert(halfNumber == 1 || halfNumber == 2);
            assert(teams[i]->inGameTeamPtr);

            for (int j = 0; j < 11; j++, pl++) {
                bPl[j].shirtNumber = pl->shirtNumber;
                bPl[j].face = pl->face;
                bPl[j].position = pl->position;
                bPl[j].isMarked = j == teams[i]->inGameTeamPtr->markedPlayer;
                bPl[j].isInjured = (*teams[i]->players)[j]->injuryLevel == -2;
                bPl[j].injury = (pl->injuriesBitfield >> 5) & 7;
                if ((*teams[i]->players)[j]->cards < 0)
                    bPl[j].cards = -1;
                else if ((*teams[i]->players)[j]->cards > 0)
                    bPl[j].cards = 1;
                else
                    bPl[j].cards = 0;
            }
        }

        subs->subsState = subsState;
        WriteToLog(LM_SUBS_MENU, "plToBeSubstitutedPos = %d, plToEnterGameIndex = %d, plToBeSubstitutedOrd = %d",
            plToBeSubstitutedPos, plToEnterGameIndex, plToBeSubstitutedOrd);
        subs->plToBeSubstitutedPos = plToBeSubstitutedPos;
        assert(plToBeSubstitutedPos <= 11);
        subs->plToEnterGameIndex = plToEnterGameIndex;
        assert(plToEnterGameIndex <= 16);
        subs->plToBeSubstitutedOrd = plToBeSubstitutedOrd;
        subs->selectedPlayerInSubs = selectedPlayerInSubs;
        subs->frameCount = frameCount;
        subs->enqueueSample = hookFlags.enqueueSubstituteSample;
        packetLength += sizeof(SubsMenuData);
    } else if (hookFlags.showingFormationMenu) {
        FormationMenuData *formation = (FormationMenuData *)&benchInfo[1];
        formation->selectedFormationEntry = selectedFormationEntry;
        packetLength += sizeof(FormationMenuData);
    }

    hookFlags.showingSubsMenu = hookFlags.showingFormationMenu = false;
    assert(m_benchSpritesIndex < (int)sizeofarray(m_benchSpritesStack));
    m_benchSpritesIndex = -1;
    hookFlags.enqueueSubstituteSample = hookFlags.enqueueTacticsChangedSample = false;
    return packetLength;
}

/** PackScorers

    scorers     -> array of SWOS scorer structures for one team
    destScorers -> our internal byte packed structure to be filled, it's a part of watcher packet for sending
    numScorers  -> will contain number of scorers found
    teamNumber  -  number of team whose scorers we're packing

    Read SWOS records about scorers in one team and pack it up to our structure. Return pointer in
    destScorers that we advanced since it can be variable length.
*/
static byte *PackScorers(const SWOS_ScorerInfo *scorers, byte *destScorers, byte *numScorers, int teamNumber)
{
    byte *goalTypesPtr;
    dword goalTypes, goalTypesSize;
    *numScorers = 0;

    for (size_t i = 0; i < MAX_SCORERS; i++) {
        if (scorers[i].shirtNum) {
            /* own goal scorers will have 100 added */
            bool ownGoalScorer = scorers[i].shirtNum >= 100;
            uint index = GetIndex(teamNumber == (1 ^ ownGoalScorer) ? leftTeamPtr : rightTeamPtr,
                scorers[i].shirtNum - (-ownGoalScorer & 100));
            WriteToLog(LM_WATCHER, "Scorer : %d, index %d", scorers[i].shirtNum, index);
            assert(scorers[i].numGoals <= 10);

            /* save combined shirt number and number of goals scored */
            int numGoals = *destScorers = scorers[i].numGoals & 0x0f;
            *destScorers++ |= index << 4;

            /* reserve space for goal types, and fill it in later */
            goalTypesPtr = destScorers;
            destScorers += goalTypesSize = getGoalTypesSize(numGoals);
            goalTypes = 0;

            for (int j = 0; j < numGoals; j++) {
                goalTypes = (goalTypes << 2) | (scorers[i].goals[j].type & 3);
                *(dword *)destScorers = scorers[i].goals[j].time;
                destScorers += 4;
                WriteToLog(LM_WATCHER, "   %d: type = %d time = %#x", j, goalTypes & 3, scorers[i].goals[j].time);
                assert(scorers[i].goals[j].type <= 2);
            }

            /* fill in goal types now that we have all the info */
            switch (goalTypesSize) {
            default:
            case 1:
                *goalTypesPtr = goalTypes;
                break;

            case 2:
                *(word *)goalTypesPtr = goalTypes;
                break;

            case 4:
                *(dword *)goalTypesPtr = goalTypes;
                break;
            }
            ++*numScorers;
        }
    }

    return destScorers;
}

/** SetTimeSprite

    gameTime - 3 digits of game time stored in 3 bytes (low byte is least significant), with values 0..9

    Render corresponding digits to time sprite.
*/
static void SetTimeSprite(dword gameTime)
{
    int firstDigit = (gameTime >> 8) & 0xff;
    int secondDigit = (gameTime >> 16) & 0xff;
    int thirdDigit = (gameTime >> 24) & 0xff;

    assert(firstDigit >= 0 && firstDigit < 10 && secondDigit >= 0 && secondDigit < 10 &&
        thirdDigit >= 0 && thirdDigit < 10);

    D1 = 0;  /* x offset */
    D2 = 0;
    D3 = SPR_8_MINS;

    if (firstDigit) {
        D3 = SPR_118_MINS;
        D1 = 6;
    }

    if (secondDigit || firstDigit) {
        D0 = SPR_TIME_BIG_0 + secondDigit;
        D3 = SPR_88_MINS;
        calla(CopySprite);
        D1 += 6;
    }

    D0 = SPR_TIME_BIG_0 + thirdDigit;
    calla(CopySprite);
}

static void DrawFormationMenu(const FormationMenuData *formationData)
{
    selectedFormationEntry = formationData->selectedFormationEntry;
    deltaColor = 16;    /* DrawFormationMenu is expecting this */
    calla(SWOS_DrawFormationMenu);
}

/** SetupTeamForSubs

    team         -> team which we're setting up
    benchPlayers -> received data about the players

    Watcher only procedure. Set up the team using data received data about the players.
    Combine it with local player name (to avoid sending it).
*/
static void SetupTeamForSubs(TeamGeneralInfo *team, const BenchMenuPlayer *benchPlayers)
{
    PlayerGame *playersGame = team->inGameTeamPtr->players;
    assert(team->inGameTeamPtr);
    HexDumpToLog(LM_WATCHER, benchPlayers, 16 * sizeof(BenchMenuPlayer), "bench menu players");

    /* restore player team - as much as is needed to display menu correctly */
    team->inGameTeamPtr->markedPlayer = -1;

    /* fill info for players currently on the field */
    for (int i = 0; i < 11; i++) {
        PlayerGame *pl = &playersGame[i];
        const BenchMenuPlayer *plBench = &benchPlayers[i];

        if (plBench->shirtNumber != pl->shirtNumber) {
            /* players seem to have changed positions, swap names*/
            char tmpName[sizeof(pl->shortName)];
            const int nameLength = sizeof(tmpName);

            /* search through all 16 players in case there were substitutions */
            int j;
            for (j = i; j < 16; j++)
                if (plBench->shirtNumber == playersGame[j].shirtNumber)
                    break;

            assert(j < 16);
            if (j >= 16)
                continue;

            /* found corresponding player at position j, swap with i-th */
            memcpy(tmpName, playersGame[j].shortName, nameLength);
            memcpy(playersGame[j].shortName, pl->shortName, nameLength);
            memcpy(pl->shortName, tmpName, nameLength);
            playersGame[j].shirtNumber = pl->shirtNumber;
        }

        /* copy received data to corresponding player */
        WriteToLog(LM_SUBS_MENU, "Player %d (%s), shirtNumber = %d, face = %d, position = %d, isMarked = %d, "
            "isInjured = %d, injury = %#x, cards = %d", i, pl->shortName, plBench->shirtNumber, plBench->face,
            plBench->position, plBench->isMarked, plBench->isInjured, plBench->injury, plBench->cards);

        pl->face = plBench->face;
        pl->position = plBench->position;

        if (plBench->isMarked) {
            assert(team->inGameTeamPtr->markedPlayer == -1);
            team->inGameTeamPtr->markedPlayer = i;
        }

        (*team->players)[i]->injuryLevel = plBench->isInjured ? -2 : 0;
        pl->injuriesBitfield &= 0x1f;
        pl->injuriesBitfield |= plBench->injury << 5;
        (*team->players)[i]->cards = plBench->cards;
        pl->shirtNumber = plBench->shirtNumber;
    }
}

static void DrawSubstitutesMenu(const SubsMenuData *subsData)
{
    SetupTeamForSubs(&leftTeamData, subsData->players[0]);
    SetupTeamForSubs(&rightTeamData, subsData->players[1]);
    subsState = subsData->subsState;
    plToBeSubstitutedPos = subsData->plToBeSubstitutedPos;
    plToEnterGameIndex = subsData->plToEnterGameIndex;
    plToBeSubstitutedOrd = subsData->plToBeSubstitutedOrd;
    selectedPlayerInSubs = subsData->selectedPlayerInSubs;
    WriteToLog(LM_SUBS_MENU, "plToBeSubstitutedPos = %d, plToEnterGameIndex = %d, plToBeSubstitutedOrd = %d",
        plToBeSubstitutedPos, plToEnterGameIndex, plToBeSubstitutedOrd);
    frameCount = subsData->frameCount;

    if (subsData->enqueueSample)
        calla(EnqueueSubstituteSample);

    deltaColor = 16;    /* DrawSubstitutesMenu is expecting this */
    calla(SWOS_DrawSubstitutesMenu);
}

static void ApplyBenchData(const BenchInfo *benchInfo)
{
    HexDumpToLog(LM_WATCHER, benchInfo, sizeof(BenchInfo), "bench info");
    benchTeam = benchInfo->benchTeam ? &rightTeamData : &leftTeamData;
    benchTeamGame = benchTeam->teamNumber == 2 ? rightTeamPtr : leftTeamPtr;

    if (benchInfo->showingSubsMenu)
        DrawSubstitutesMenu((const SubsMenuData *)&benchInfo[1]);
    else if (benchInfo->showingFormationMenu)
        DrawFormationMenu((const FormationMenuData *)&benchInfo[1]);
}

/** GetSurname

    team  -> in-game team to search player in
    index -  ordinal number of player

    Return player surname without initials. No need to lookup since player's client
    will convert shirt number for us (SWOS only keeps shirt numbers of scorers).
*/
static const char *GetSurname(const TeamGame *team, int index)
{
    char *surname = ((PlayerGame *)((char *)team + offsetof(TeamGame, players)))[index].shortName;
    assert(index >= 0 && index <= 15);

    /* skip possible initial */
    if (surname[0] && surname[1] && surname[1] == '.' && surname[2] == ' ')
        surname += 3;

    return surname;
}

/** GetIndex

    team        -> in-game team of the player
    shirtNumber -  shirt number of the player

    Return index - ordinal of the player with the given shirt number in team file.
    It is in range [0..15].
*/
static int GetIndex(const TeamGame *team, int shirtNumber)
{
    PlayerGame *pl = (PlayerGame *)((char *)team + offsetof(TeamGame, players));
    for (int i = 0; i < 16; i++, pl++)
        if (pl->shirtNumber == shirtNumber)
            return i;

    /* This has to succeed or it would mean SWOS added scorers incorrectly. */
    assert_msg(false, "Non existent scorer registered.");
    return 0;
}

/* Render received scorer list into scorer sprites (per team function). */
static byte *ApplyScorerList(byte *scorers, int numScorers, int teamNumber, Sprite *scorerSprites)
{
    int i, x, goalTypesSize;
    dword *goalTimes, goalTypes;
    assert(numScorers >= 0 && numScorers <= (int)MAX_SCORERS);
    assert(teamNumber == 1 || teamNumber == 2);
    WriteToLog(LM_WATCHER, "Applying scorer list for team %d", teamNumber);

    for (i = 0; i < numScorers; i++) {
        uint index = *scorers >> 4;
        uint numGoals = *scorers++ & 0x0f;

        if (numGoals <= 4) {
            goalTypesSize = 1;
            goalTypes = *scorers;
        } else if (numGoals <= 8) {
            goalTypesSize = 2;
            goalTypes = *(word *)scorers;
        } else {
            goalTypesSize = 4;
            goalTypes = *(dword *)scorers;
        }

        /* if it's own goal look up other team, otherwise this one */
        TeamGame *scorerTeam = (teamNumber - 1) ^ ((goalTypes >> ((2 * (numGoals - 1)) & 3) != 2)) ? leftTeamPtr : rightTeamPtr;
        WriteToLog(LM_WATCHER, "Scorer %d, index = %d, name = '%s', goals scored = %d",
            i, index, GetSurname(scorerTeam, index), numGoals);
        assert(numGoals <= 10 && scorerSprites->pictureIndex != -1);

        if (scorerSprites->pictureIndex == -1) {
            WriteToLog(LM_WATCHER, "Found scorer sprite with -1 picture index! index %d", i);
            continue;
        }

        D0 = scorerSprites->pictureIndex;
        calla(ClearSprite);
        x = Text2Sprite(0, 0, scorerSprites->pictureIndex, GetSurname(scorerTeam, index), smallCharsTable) + 5;
        assert(x > 0);
        goalTimes = (dword *)(scorers + goalTypesSize);

        for (uint j = 0; j < numGoals; j++) {
            static char ** const goalScoredPrefixTables[2] = { firstGoalScoredTable, alreadyScoredTable };
            char *goalTimeStr = goalScoredPrefixTables[j != numGoals - 1][(goalTypes >> (2 * (numGoals - j - 1))) & 3];
            *(dword *)goalTimeStr = *goalTimes++;
            if (*++goalTimeStr == '0')  /* skip leading zeros */
                goalTimeStr++;
            if (*goalTimeStr == '0')
                goalTimeStr++;
            WriteToLog(LM_WATCHER, "  Goal %d, type = %d, str = '%s', sprite = %d", j,
                (goalTypes >> (2 * (numGoals - j - 1))) & 3, goalTimeStr, scorerSprites->pictureIndex);

            /* update current offset */
            if ((x = Text2Sprite(x, 0, scorerSprites->pictureIndex, goalTimeStr, smallCharsTable)) < 0) {
                D0 = (++scorerSprites)->pictureIndex;
                calla(ClearSprite);
                x = 0;
            }
        }

        scorers = (byte *)goalTimes;
        scorerSprites++;
    }

    /* goals shouldn't disappear, but make those sprites invisible just in case */
    for (; i < (int)MAX_SCORERS; i++, scorerSprites++)
        scorerSprites->visible = scorerSprites->pictureIndex != -1;

    return scorers;
}

/* Sign extend 10-bit value to full 32-bit integer. */
static int SignExtend10Bits(int val)
{
    struct { signed int x:10; } s;
    return s.x = val;
}

static bool IsBenchSprite(int spriteNo)
{
    return spriteNo >= SPR_COACH1_SITTING_1 && spriteNo <= SPR_TEAM_2_RESERVE_6;
}

/* Prepare contents of this watcher packet for rendering. */
static void ApplyWatcherPacket(const WatcherPacket *packet)
{
    int i;
    byte *scorers;
    WriteToLog(LM_WATCHER, "Applying watcher packet for frame %d", packet->frameNo);
    WriteToLog(LM_WATCHER, "Camera x = %d, y = %d", cameraX, cameraY);
    memcpy(&cameraXFraction, &packet->cameraX, sizeof(dword));
    memcpy(&cameraYFraction, &packet->cameraY, sizeof(dword));
    calla(ScrollToCurrent);
    animPatternsState = packet->flags & 0x0f;
    stoppageTimer = packet->stoppageTimer;
    WriteToLog(LM_WATCHER, "showingStats = %d, animPatternsState = %d", (packet->flags >> 4) & 1, animPatternsState);

    /* fix up time */
    SetTimeSprite(packet->gameTime);
    assert(packet->numSprites <= MAX_SPRITES);

    if (!(packet->flags & SHOWING_STATS_FLAG)) {
        /* only modify animated patterns if we're not showing stats */
        replayState = true;     /* DrawAnimatedPatterns will blindly accept patterns state if this is true ;) */
        calla(DrawAnimatedPatterns);
        replayState = false;    /* but better restore it or crash and burn */

        /* handle scorers before sprites so they will be ready for rendering */
        if (!(packet->flags & SHOWING_BENCH_FLAG)) {
            scorers = ApplyScorerList((byte *)&packet->sprites[packet->numSprites],
                packet->numScorers & 0x0f, 1, team1Scorer1Sprite);
            ApplyScorerList(scorers, packet->numScorers >> 4, 2, team2Scorer1Sprite);
        }
    }

    for (i = 0; i < packet->numSprites; i++) {
        deltaColor = 0;

        /* remember to reapply sign of x and y coordinates */
        D1 = SignExtend10Bits(packet->sprites[i] & 0x3ff);
        D2 = SignExtend10Bits((packet->sprites[i] >> 10) & 0x3ff);
        D0 = (packet->sprites[i] >> 20) & 0x7ff;

        /* handle player numbers greater than 16, they will start from SPR_MAX */
        if (D0 >= SPR_MAX && D0 < SPR_MAX + 256) {
            PrintSmallNumber(D0 - SPR_MAX, D1, D2, true);
            continue;
        }

        if (D0 >= SPR_MAX)   /* just in case... */
            continue;

        /* incy wincy hack so we don't need to send separate bit - draw bench sprites with deltaColor 16 */
        if (IsBenchSprite(D0))
            deltaColor = 16;

        if (D0 >= SPR_BIG_S_FRAME_00 && D0 <= SPR_BIG_S_FRAME_31)
            deltaColor = 0x70;

        if (D0 == SPR_SQUARE_GRID_FOR_RESULT)
            calla(DarkenRectangle);
        else
            DrawSprite16Pixels((packet->sprites[i] >> 31) ? -1 : 0);
    }

    deltaColor = 0;
    ApplyAdData(packet);

    /* apply statistics only after drawing sprites so they remain in the back */
    if (packet->flags & (SHOWING_STATS_FLAG | AUTO_SHOWING_STATS_FLAG)) {
        memcpy(&team1StatsData, packet->sprites + packet->numSprites, 2 * sizeof(TeamStatsData));
        calla(DrawStatistics);
    } else if (packet->flags & SHOWING_BENCH_FLAG) {
        ApplyBenchData((BenchInfo *)&packet->sprites[packet->numSprites]);
    }
}

/** GetWatcherPacketsData

    Returns various data about watcher packets.

    Arguments:
    emptySpotIndex[out] - will receive index of empty spot, or -1 if everything is full
    numPackets[out]     - will receive number of packets in buffer
    frameNo[in]         - frame number of packet to look for

    Return value:
    Pointer to packet with frame number specified, or oldest watcher packet if queue
    doesn't contain such packet, or first packet if queue is empty.
*/
static WatcherPacket *GetWatcherPacketsData(int *emptySpotIndex, int *numPackets, int frameNo)
{
    int i, oldestFrame = INT_MAX;
    WatcherPacket *oldestPacket = m_watcherPackets;
    *emptySpotIndex = -1;
    *numPackets = 0;

    for (i = 0; i < WATCHER_PACKETS_BUFFERED; i++) {
        if (m_watcherPackets[i].type == PT_GAME_SPRITES) {
            if (m_watcherPackets[i].frameNo < oldestFrame) {
                oldestPacket = m_watcherPackets + i;
                oldestFrame = m_watcherPackets[i].frameNo;
            }
            ++*numPackets;
            if (frameNo == m_watcherPackets[i].frameNo)
                return &m_watcherPackets[i];
        } else if (*emptySpotIndex < 0)
            *emptySpotIndex = i;
    }

    return oldestPacket;
}

/* We have received a packet from the player, so put it in the buffer.
   If there's no room overwrite oldest packet. */
static void StoreWatcherPacket(const WatcherPacket *packet)
{
    int emptyPacketIndex, numPackets;
    WatcherPacket *oldestPacket;
    /* reject very old packets immediately */
    if (packet->frameNo < m_lastWatcherFrame)
        return;

    /* if we have that frame overwrite anyway */
    oldestPacket = GetWatcherPacketsData(&emptyPacketIndex, &numPackets, packet->frameNo);
    WriteToLog(LM_WATCHER, "Storing watcher packet for frame %d", packet->frameNo);

    /* for packets with auto stats flag one client might start it earlier so to avoid flickering
       try not prioritize packets with flag set */
    if (oldestPacket->frameNo == packet->frameNo &&
        (packet->flags & AUTO_SHOWING_STATS_FLAG || !(oldestPacket->flags & AUTO_SHOWING_STATS_FLAG)))
        *oldestPacket = *packet;            /* we have it already, just overwrite */
    else if (emptyPacketIndex >= 0) {       /* don't have it, if there's an empty place it there */
        m_watcherPackets[emptyPacketIndex] = *packet;
        /* get rid of oldest frame if it was rendered, to avoid it rendering it more than it's necessary */
        if (oldestPacket->rendered)
            oldestPacket->type = -1;
    }
}
