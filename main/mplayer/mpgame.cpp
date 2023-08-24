/** mpgame.cpp

    The actual game packet exchange implementation.
*/

#include <limits.h>
#include "qalloc.h"
#include "dosipx.h"
#include "mplayer.h"
#include "mppacket.h"
#include "mpwatch.h"
#include "mptact.h"

static TeamGeneralInfo * const m_teams[2] = { &leftTeamData, &rightTeamData };

static int m_currentFrameNo;            /* current frame we are in */
static int m_nextSendFrameNo;           /* this is the next frame we need to send our input in */
static int m_lastAppliedFrame;          /* guard from applying same frame twice */
static byte m_sendLastFrame;            /* keep sending last frame until ack-ed */
static byte m_gameStatus;               /* how did the game end */

struct ResendFrame {
    int frameNo;
    word controls;
    byte state;
};

static ResendFrame m_resendFrames[3];   /* must keep at least 3 last frames */
static int m_resendIndex;

static bool __attribute__((used)) ReceiveFrame() asm("ReceiveFrame");
static void __attribute__((used)) SendNextFrame() asm("SendNextFrame");

static void SetAllResendFramesState(byte state)
{
    for (size_t i = 0; i < sizeofarray(m_resendFrames); i++)
        m_resendFrames[i].state = state;
}

/* in case of packet drop we might need to resend this later */
static void RegisterFrameForResend(int frameNo, word controls, byte state)
{
    /* avoid duplicates, as it is assumed that each index equals to one frame */
    if (m_resendFrames[m_resendIndex].frameNo != frameNo) {
        ResendFrame *rf = &m_resendFrames[m_resendIndex];
        rf->frameNo = frameNo;
        rf->controls = controls;
        rf->state = state;
        m_resendIndex = (m_resendIndex + 1) % sizeofarray(m_resendFrames);
    }
}

static inline ResendFrame *getPreviousResendFrame(int *index)
{
    return &m_resendFrames[*index = (*index - 1 + sizeofarray(m_resendFrames)) % sizeofarray(m_resendFrames)];
}

#define MAX_SKIP_FRAMES     3

static signed char m_skipFrames;

static int m_playerNo asm("m_playerNo") __attribute__((used));  /* 1, 2 or 0 for watcher */
static IPX_Address *m_playerAddresses;
static const IPX_Address *m_otherPlayerAddress;
static int m_numWatchers;
static const IPX_Address *m_watcherAddresses;

#define MAX_BURST_PACKETS   3
#define SLOW_SEND_TICKS    20

static int m_burstPacketsSent;
static word m_lastSendTime;
static byte m_sentThisFrame;    /* prevent sending more than 1 packet per frame */

static byte m_stateQueued;      /* bitfield of states to apply at next allowed frame */
static byte m_mainLoopRan asm("m_mainLoopRan") __attribute__((used));

static word m_savedNumLoopsJoy2;

static byte m_pl2KeyboardWasActive;     /* be careful not to clash with pl2 on keyboard */

extern byte pl2Keyboard;
extern "C" void SetSecondPlayerOnKeyboard(unsigned char);

/* state bit masks */
#define STATE_SHOWING_STATS   1     /* obsolete */
#define STATE_PAUSED          2
#define STATE_SHOW_STATS      4
#define STATE_BENCH1_CALLED   8
#define STATE_BENCH2_CALLED  16
#define STATE_ABORT1         32
#define STATE_ABORT2         64
#define STATE_GAME_ENDED    128

#define STATE_ABORT_MASK    (STATE_ABORT1 | STATE_ABORT2 | STATE_GAME_ENDED)

#pragma pack(push, 1)
/* Our frame structure used for sending and receiving frames. */
struct Frame {
    word type;
    word controls;
    int frameNo;
    byte state;
#ifdef DEBUG
    byte filler[3];     /* debug support for cross-network assert ;) */
    dword team1Hash;
    dword team2Hash;
    int hashFrame;
#endif
};
#pragma pack(pop)

/* frames ready to be rendered locally - can only render if frame ready here */
static Frame framesToRender[4 * (MAX_SKIP_FRAMES + 3) + 2]; /* size determined empirically :P */

#ifdef DEBUG
/* support for asserting state hashes across the network, and also for keeping
   certain number of states to be able to see the point where states mismatch */
struct FrameHash {
    dword team1Hash;
    dword team2Hash;
    TeamGeneralInfo team1Data;
    TeamGeneralInfo team2Data;
    int frameNo;
};

static FrameHash m_frameHashes[20];
static int m_frameHashIndex;        /* points to one after the last one */

static dword GetActualTeam1Hash()
{
    return simpleHash((char *)&leftTeamData + 24, 145 - 24);
}

static dword GetActualTeam2Hash()
{
    return simpleHash((char *)&rightTeamData + 24, 145 - 24);
}

static void StoreCurrentHash()
{
    /* prevent same frame hash getting stored twice during pause and stats */
    int lastHashIndex = ((m_frameHashIndex - 1) + sizeofarray(m_frameHashes)) % sizeofarray(m_frameHashes);
    if (m_frameHashes[lastHashIndex].frameNo != m_currentFrameNo) {
        m_frameHashes[m_frameHashIndex].team1Hash = GetActualTeam1Hash();
        m_frameHashes[m_frameHashIndex].team2Hash = GetActualTeam2Hash();
        m_frameHashes[m_frameHashIndex].team1Data = leftTeamData;
        m_frameHashes[m_frameHashIndex].team2Data = rightTeamData;
        m_frameHashes[m_frameHashIndex++].frameNo = m_currentFrameNo;
        m_frameHashIndex %= sizeofarray(m_frameHashes);
    }
}

static FrameHash *GetHashAtPos(int spot)
{
    if (abs(spot) >= sizeofarray(m_frameHashes))
        return nullptr;
    return &m_frameHashes[(spot + m_frameHashIndex - 1 + sizeofarray(m_frameHashes)) % sizeofarray(m_frameHashes)];
}

static void DumpSavedStates()
{
    /* index is already set to the oldest element - one beyond last added */
    int cur = m_frameHashIndex;
    for (size_t i = 0; i < sizeofarray(m_frameHashes); i++) {
        WriteToLog("*** Dumping saved teams for frame %d", m_frameHashes[cur].frameNo);
        HexDumpToLog(&m_frameHashes[cur].team1Data, sizeof(m_frameHashes[cur].team1Data), "left team");
        HexDumpToLog(&m_frameHashes[cur].team2Data, sizeof(m_frameHashes[cur].team2Data), "right team");
        cur = (cur + 1) % sizeofarray(m_frameHashes);
    }
}

/** [DEBUG]

    Do sanity checks on received packet, abort game and dump data if anything invalid is detected.
*/
static void VerifyReceivedPacket(const char *packet, int frameIndex, int frameNo)
{
    /* state might be altered in case of game-ending packets */
    if (framesToRender[frameIndex].type == PT_GAME_CONTROLS &&
        !(framesToRender[frameIndex].state & STATE_ABORT_MASK) &&
        memcmp(&framesToRender[frameIndex], packet, offsetof(Frame, state) + sizeof(Frame::state))) {
        WriteToLog("Different frames received for frameNo %d!", frameNo);
        HexDumpToLog(packet, sizeof(Frame), "newly received packet");
        HexDumpToLog(&framesToRender[frameIndex], sizeof(Frame), "previous packet");
        assert_msg(false, "Different frames received!");
    } else {
        Frame *f = (Frame *)packet;
        int hashFrameDiff = f->hashFrame - m_currentFrameNo;
        FrameHash *fh = GetHashAtPos(hashFrameDiff);

        if (fh && (hashFrameDiff < 1 || (hashFrameDiff == 1 && m_currentFrameNo != teamSwitchCounter))) {
            dword h1 = fh->team1Hash;
            dword h2 = fh->team2Hash;
            /* if this one is referencing next frame, which we might be in, get current hashes */
            if (hashFrameDiff == 1) {
                WriteToLog("Comparing to current hashes.");
                h1 = GetActualTeam1Hash();
                h2 = GetActualTeam2Hash();
            }

            if (f->team1Hash != h1 || f->team2Hash != h2) {
                WriteToLog("Packet received (%d), hash mismatch for frame %d", f->frameNo, f->hashFrame);
                WriteToLog("Saved hashes: %#x and %#x", h1, h2);
                WriteToLog("Received hashes: %#x and %#x", f->team1Hash, f->team2Hash);
                HexDumpToLog(&leftTeamData, 145, "left team");
                HexDumpToLog(&rightTeamData, 145, "right team");
                WriteToLog("State from last %d frames:\n----------------------------------------\n", sizeofarray(m_frameHashes));
                DumpSavedStates();
            }

            assert(h1 == f->team1Hash);
            assert(h2 == f->team2Hash);
        } else
            WriteToLog("Frame %d is out of range of hash history.", f->hashFrame);
    }
}
#else
#define VerifyReceivedPacket(a, b, c)   ((void)0)
#define StoreCurrentHash()              ((void)0)
#define DumpSavedStates()               ((void)0)
#endif

/* necessary hooks for all this mumbo jumbo to work */
static void HookMainLoop();
void HookFlip() asm("HookFlip");

static void ShowStatsLoop();
static void PausedLoop();

static bool IsBench1Allowed();
static bool IsBench2Allowed();

static void InitFrame(Frame *f, word controls, int frameNo, byte state)
{
    f->type = PT_GAME_CONTROLS;
    f->controls = controls;
    f->frameNo = frameNo;
    f->state = state;
#ifdef DEBUG
    f->team1Hash = GetActualTeam1Hash();
    f->team2Hash = GetActualTeam2Hash();
    f->filler[0] = 'Z'; f->filler[1] = 'K'; f->filler[2] = 'Z';
    /* if this frame was already rendered, then it's the state of next frame */
    f->hashFrame = m_currentFrameNo + (m_currentFrameNo != teamSwitchCounter);
#endif
}

static void SendFrame(int frameNo, word controls, byte state)
{
    Frame frame;
    WriteToLog(LM_GAME_LOOP, "Sending frame %d, controls = %#x, state = %#x", frameNo, controls, state);
    InitFrame(&frame, controls, frameNo, state);
    SendSimplePacket(m_otherPlayerAddress, (char *)&frame, sizeof(frame));

    /* inform watchers of aborting also */
    if (state & STATE_ABORT_MASK)
        for (int i = 0; i < m_numWatchers; i++)
            SendSimplePacket(&m_watcherAddresses[i], (char *)&frame, sizeof(frame));
}

/* We are applying controls for this frame, should be only possible spot where it is allowed.
   Do not touch control word, or it will get propagated into future frames. */
static void ApplyControls(word controls, int playerNo)
{
    static word *const statusWords[] = { &g_joy1Status, &g_joy2Status };
    assert(playerNo == 0 || playerNo == 1);
    *statusWords[playerNo] = controls;
}

static void ApplyFrame(const Frame *frame)
{
    bool playerTurn = (teamSwitchCounter & 1) ^ (m_playerNo == 2);    /* determine which player gets these controls */

    WriteToLog(LM_GAME_LOOP, "Applying frame %d, state = %#x, controls = %#x (player %d)",
        frame->frameNo, frame->state, frame->controls, playerTurn + 1);
    assert(m_playerNo != 0);

    ApplyControls(frame->controls, playerTurn);

    /* State change was generated blindly, possibly many frames ago. Now we have to
       decide whether we apply it or not, based on the current frame conditions.
       Since simulation is synchronized, the other client will reach the same conclusion. */

    if (!paused && !showingStats) {
        if (frame->state & STATE_BENCH1_CALLED && IsBench1Allowed())
            bench1Called = true;
        if (frame->state & STATE_BENCH2_CALLED && IsBench2Allowed())
            bench2Called = true;
    }

    if (frame->state & STATE_PAUSED)
        if (!showingStats)  /* no pause if showing stats */
            paused ^= 1;

    /* It isn't enough to just set variable to true or false - AllowStatistics()
       must be called to negate resultTimer, then result sprite goes away,
       otherwise it's smeared all over the screen when stats show up. */
    if (frame->state & STATE_SHOW_STATS) {
        if (showingStats) {
            calln(StatisticsOff);
        } else if (gameStatePl != 100 && !inSubstitutesMenu && !replayState && !paused &&
            (gameState < ST_STARTING_GAME || gameState > ST_GAME_ENDED)) {
            calln(AllowStatistics);
        }
    }
}

/* Try to read next frame and store it into framesToRender array, to be ready when the time for rendering comes.
   Return true if packet processed, false if queue empty. */
static bool __attribute__((used)) ReceiveFrame()
{
    char *packet;
    int length;
    IPX_Address node;
    bool abort;

    if ((packet = ReceivePacket(&length, &node))) {
        /* check packet source */
        if (!AddressMatch(&node, &m_playerAddresses[0]) && !AddressMatch(&node, &m_playerAddresses[1])) {
            WriteToLog("Received a packet from invalid player!");
            qFree(packet);
            return true;
        }

        if (GetRequestType(packet, length) == PT_GAME_CONTROLS && length == sizeof(Frame)) {
            int frameNo = ((Frame *)packet)->frameNo;
            int frameIndex = frameNo - m_currentFrameNo;
            Frame *frame = (Frame *)packet;

            /* send ack if it's the last frame - don't care if it's out of range */
            if (frame->state & STATE_ABORT_MASK) {
                /* reject invalid packets - saying that we aborted when we didn't */
                if ((m_playerNo == 1 && frame->state & STATE_ABORT1) ||
                    (m_playerNo == 2 && frame->state & STATE_ABORT2)) {
                    WriteToLog("Fake abort? m_playerNo = %d, state = %#x", m_playerNo, frame->state);
                    qFree(packet);
                    return true;
                }

                if (m_playerNo) {
                    WriteToLog("Sending last frame ack.");
                    SendFrame(frameNo, 0, STATE_GAME_ENDED);
                }

                /* abort the game asap, therefore mark all frames in queue with abort bit */
                for (size_t i = 0; i < sizeofarray(framesToRender); i++)
                    framesToRender[i].state |= frame->state & STATE_ABORT_MASK;

                /* frame's already address checked against other player */
                if (m_playerNo == 2 && frame->state & STATE_ABORT1) {
                    m_gameStatus = GS_PLAYER1_ABORTED;
                    paused = showingStats = showStats = playGame = false;
                    m_sendLastFrame = true;
                } else if (m_playerNo == 1 && frame->state & STATE_ABORT2) {
                    m_gameStatus = GS_PLAYER2_ABORTED;
                    paused = showingStats = showStats = playGame = false;
                    m_sendLastFrame = true;
                }

                if (frame->state & STATE_GAME_ENDED)    /* make it pass-through */
                    playGame = paused = showingStats = showStats = m_sendLastFrame = false;
                qFree(packet);
                return true;
            }

            assert_msg(m_playerNo != 0, "Watcher received player packet!");
            if (frameIndex >= 0 && frameIndex + 2 * m_skipFrames < (int)sizeofarray(framesToRender)) {
                VerifyReceivedPacket(packet, frameIndex, frameNo);
                framesToRender[frameIndex] = *(Frame *)packet;

                /* based on skip frames, we might copy this frame further on */
                for (int i = 1; i <= m_skipFrames && frameIndex + 2 * i < (int)sizeofarray(framesToRender); i++) {
                    int clonedFrameIndex = frameIndex + 2 * i;
                    framesToRender[clonedFrameIndex] = *(Frame *)packet;
                    /* might not be necessary to update frame number, but makes it easier to follow debug log */
                    framesToRender[clonedFrameIndex].frameNo += 2 * i;
                    framesToRender[clonedFrameIndex].state = 0; /* clear state or it will keep getting flipped */
                }

                assert_msg(frameIndex + 2 * m_skipFrames < (int)sizeofarray(framesToRender), "framesToRender too small!");
            } else {
                WriteToLog("Rejecting out of range frame: %d.", frameNo);
                if (frameIndex > 25)
                    HexDumpToLog(packet, length, "crazy wild packet o_O");
            }
        } else if (GetRequestType(packet, length) == PT_JOIN_GAME && length >= 2) {
            char *response = CreateRefuseJoinGamePacket(&length, GR_IN_PROGRRESS);
            SendSimplePacket(&node, response, length);
            WriteToLog("Some sucker trying to join now.");
        } else if (HandleWatcherPacket(packet, length, g_currentTick, &abort)) {
            assert_msg(m_playerNo == 0, "Player received watcher packet!");
            if (abort) {
                WriteToLog("Received game end packet from players.");
                paused = showingStats = showStats = playGame = false;
                m_gameStatus = GS_WATCHER_ENDED;
            }
        } else {
            WriteToLog("Invalid packet rejected in the game.");
            HexDumpToLog(packet, length, "reject packet");
        }
        qFree(packet);
    } else
        return false;

    return true;
}

static bool SendingThisFrame()
{
    return m_currentFrameNo + (m_currentFrameNo != teamSwitchCounter) + 2 + 2 * m_skipFrames >= m_nextSendFrameNo;
}

/** SendNextFrame

    Check if we're sending next frame and gather all the input and send it to the other player if so.
*/
static void __attribute__((used)) SendNextFrame()
{
    assert(m_playerNo != 0);
    if (!m_sentThisFrame && SendingThisFrame()) {
        word controls;
        int i, nextFrameIndex = m_nextSendFrameNo - m_currentFrameNo;
        Frame *sentFrame = &framesToRender[nextFrameIndex];

        /* sample real local input */
        assert(g_joyKbdWord == 4 || g_joyKbdWord == 5);
        if (g_joyKbdWord == 4)
            controls = controlWord;
        else {
            /* this will trash current joy1Status, so better save it */
            word savedJoy1Status = g_joy1Status;
            calla_ebp_safe(Joy1SetStatus);
            controls = g_joy1Status;
            g_joy1Status = savedJoy1Status;
        }

        /* fill in our known frames to be rendered */
        InitFrame(sentFrame, controls, m_nextSendFrameNo, m_stateQueued);
        RegisterFrameForResend(m_nextSendFrameNo, controls, m_stateQueued);
        SendFrame(m_nextSendFrameNo, controls, m_stateQueued);
        for (i = 1; i <= m_skipFrames; i++) {
            int clonedFrameIndex = nextFrameIndex + 2 * i;
            framesToRender[clonedFrameIndex] = *sentFrame;
            framesToRender[clonedFrameIndex].frameNo += 2 * i;
            framesToRender[clonedFrameIndex].state = 0;
        }
        m_nextSendFrameNo += 2 * m_skipFrames + 2;
        m_sentThisFrame = true;
        m_stateQueued = 0;    /* start over for next frame */
    }
}

bool IsBench1Allowed()
{
    return !replayState && (leftTeamData.playerNumber || leftTeamData.plCoachNum);
}

bool IsBench2Allowed()
{
    return !replayState && (rightTeamData.playerNumber || rightTeamData.plCoachNum);
}

/** GetControlScanPlayerNumber

    Return player number whose turn it is to have controls scanned.
*/
static int GetControlScanPlayerNumber()
{
    return m_teams[(teamSwitchCounter & 1) ^ (teamPlayingUp == 2)]->playerNumber;
}

/* Called when the game ended, on exit from main loop. Send the last frame and cleanup. */
static void GameEnded()
{
    word startTime = g_currentTick, networkTimeout = GetNetworkTimeout();
    word lastSendTime = startTime - 25;
    bool ack = false;
    char *packet = nullptr;
    int length, i;
    IPX_Address node;

    WriteToLog("GameEnded, m_gameStatus = %d", m_gameStatus);
    if (m_gameStatus == GS_WATCHER_ABORTED || m_gameStatus == GS_WATCHER_ENDED) {
        GameFinished();
        calla_ebp_safe(StopAudio);
        replaySelected = false;
        return;
    }

    if (m_gameStatus == GS_GAME_IN_PROGRESS)
        m_gameStatus = GS_GAME_FINISHED_OK;

    if (m_sendLastFrame) {
        WriteToLog("Waiting on exit ack...");
        /* keep sending the last frame until ack-ed */
        while (startTime + networkTimeout > g_currentTick) {
            if (lastSendTime + 25 <= g_currentTick) {
                WriteToLog("Sending abort game notification...");
                SendFrame(m_currentFrameNo, 0, STATE_GAME_ENDED);
                lastSendTime = g_currentTick;
            }

            if ((packet = ReceivePacket(&length, &node)) &&
                (AddressMatch(&node, &m_playerAddresses[0]) || AddressMatch(&node, &m_playerAddresses[1])) &&
                length == sizeof(Frame) && *(word *)packet == PT_GAME_CONTROLS &&
                ((Frame *)packet)->state & STATE_GAME_ENDED) {
                qFree(packet);
                WriteToLog("Received exit game confirmation.");
                ack = true;     /* got it! */
                break;
            }

            if (packet)
                qFree(packet);

            IPX_OnIdle();
        }
    }

    /* send end game packet to watchers - once, and hope for the best
       if both packets get lost they will simply timeout */
    if (m_numWatchers) {
        const char *packet = GetEndGameWatcherPacket(&length);
        for (i = 0; i < m_numWatchers; i++)
            SendSimplePacket(&m_watcherAddresses[i], packet, length);
    }

    if (m_sendLastFrame && !ack)
        m_gameStatus = GS_GAME_DISCONNECTED;

    GameFinished();
    WriteToLog(LM_GAME_LOOP, "Game has ended, result code is %d.", m_gameStatus);
    replaySelected = false; /* no replay, we're done */
    calla_ebp_safe(StopAudio);
}

/** HandleMPKeys

    key - converted ASCII code of last pressed key, or 0 if nothing

    Called from MainKeysCheck(). Filter keys during multiplayer game, returning
    converted key that will replace the original value.
*/
int HandleMPKeys(int key)
{
    /* Don't touch anything if we're not in a multiplayer game; if we are,
       disable anything that disrupts game flow. Capture game state changing keys
       straight into queue, and never let them affect the game directly. Do not test
       if they're allowed as they will possibly be used many frames ahead. */
    if (m_playerAddresses) {
        switch (lastKey) {  /* need raw scan codes for these */
        case KEY_PAGE_DOWN:
        case KEY_PAGE_UP:
            if (lastKey == KEY_PAGE_DOWN)
                m_stateQueued |= STATE_BENCH1_CALLED;
            else
                m_stateQueued |= STATE_BENCH2_CALLED;
            lastKey = 0;    /* make sure it's cleared */
            return 0;
        }

        switch (key) {
        case 'R':           /* block these out completely */
        case 'H':
        case ' ':
            return 0;

        case 'S':
            m_stateQueued |= STATE_SHOW_STATS;
            return 0;

        case 'P':
            m_stateQueued |= STATE_PAUSED;
            return 0;

        case 1:             /* escape */
            WriteToLog("Escape pressed, aborting");
            m_sendLastFrame = true;
            switch (m_playerNo) {
                case 0:
                    m_gameStatus = GS_WATCHER_ABORTED;
                    break;

                case 1:
                    m_gameStatus = GS_PLAYER1_ABORTED;
                    SendFrame(m_nextSendFrameNo, 0, STATE_ABORT1 | ST_GAME_ENDED);
                    SetAllResendFramesState(STATE_ABORT1);
                    break;

                case 2:
                    m_gameStatus = GS_PLAYER2_ABORTED;
                    SendFrame(m_nextSendFrameNo, 0, STATE_ABORT2 | ST_GAME_ENDED);
                    SetAllResendFramesState(STATE_ABORT2);
                    break;

                default:
                    assert_msg(0, "Impossible value for m_playerNo!");
            }
            break;
        }
    }

    return key;
}

/** PollNetworkAndWaitRetrace

    Acknowledge any packet that arrives while we're fading in and out before the game, since
    other player might be waiting on our confirmation.
*/
static void PollNetworkAndWaitRetrace()
{
    DismissIncomingPackets();
    calla(WaitRetrace);
}

static void HookNetworkPoll()
{
    if (m_playerNo == 1) {
        PatchCall(FadeOut, 0x2c, PollNetworkAndWaitRetrace);
        PatchCall(VersusInit, 0x255, PollNetworkAndWaitRetrace);
        PatchCall(ShowStadiumInit, 0x4ed, PollNetworkAndWaitRetrace);
    }
}

static void UnhookNetworkPoll()
{
    if (m_playerNo == 1) {
        PatchDword(FadeOut, 0x2c, 0xffffffc0);
        PatchDword(VersusInit, 0x255, 0xfffba7f5);
        PatchDword(ShowStadiumInit, 0x4ed, 0xfffbac68);
    }
}

/* Hook main loop to enable blocking if next frame hasn't arrived yet. */
void HookMainLoop()
{
    if (m_mainLoopRan)
        OnGameLoopEnd();
    else
        UnhookNetworkPoll();    /* this is only needed until we start the game */

    if (!paused && !showingStats) {
        m_mainLoopRan = 0;
        OnGameLoopStart();
        m_mainLoopRan++;
    }
    calln(ReadTimerDelta);      /* SWOS doesn't use registers anyway, call the version that doesn't save them */
}

extern void CheckForFastReplay() asm("CheckForFastReplay");
/* Patch waiting for retrace start, as we might spend some time there, and insert network check. */
asm(
"HookFlip:                      \n"
    "call CheckForFastReplay    \n"     /* we overwrote this */
    "mov  al, m_mainLoopRan     \n"     /* prevent Flip() from getting called before OnGameLoopStart() in first frame */
    "test al, al                \n"
    "jz   .return               \n"

    "mov  eax, m_playerNo       \n"     /* watchers don't need this */
    "test eax, eax              \n"
    "jz   .receive_frame        \n"

    "call SendNextFrame         \n"

".receive_frame:                \n"
    "call ReceiveFrame          \n"
    ";call IPX_OnIdle           \n"

".return:                       \n"
    "ret                        \n"
);

/** HookUpdateStatistics

    Send watcher packets from here, or apply received. This is last unconditionally called function
    in the game loop - it's important to do it at the end so we can send fresh bench data as needed.
*/
static void HookUpdateStatistics()
{
    char *watcherPacket;
    int watcherPacketSize;
    if (m_playerNo != 0) {
        if (m_numWatchers) {
            int i;
            if (!(watcherPacket = CreateWatcherPacket(&watcherPacketSize, m_currentFrameNo)))
                return;

            WriteToLog(LM_WATCHER, "Sending packets to watchers...");
            for (i = 0; i < m_numWatchers; i++)
                SendSimplePacket(&m_watcherAddresses[i], watcherPacket, watcherPacketSize);
        }
    } else
        ApplyWatcherPacket();

    calla(UpdateStatistics);
}

/** HookGetPlayerAtIndex

    in:
         D0 -  team number (ignored)
         D1 -  index of a player whose index we are looking for
         A2 -> TeamFile of the player (this will only come from this patched location)
    out:
         D1 -  player position (index in team file)
         A0 -> player pointer, offsetted backwards by the size of TeamFile header

    Replace this function and return real data based on concrete team, instead of relying on search by team id,
    which can lead to returning wrong player structure when both players pick the same team.
*/
static void HookGetPlayerAtIndex()
{
    const auto team = (TeamFile *)A2;
    D1 = team->playerOrder[word(D1)];
    A0 = (dword)&team->players[D1] - TeamFileHeaderSize;
}

/* No input will be registered after this call. */
static void DisableInput()
{
    *(word *)((byte *)Player1StatusProc + 0x8) = 0x15eb;
    *(word *)((byte *)Player2StatusProc + 0x8) = 0x15eb;
}

static void EnableInput()
{
    *(word *)((byte *)Player1StatusProc + 0x8) = 0x1075;
    *(word *)((byte *)Player2StatusProc + 0x8) = 0x1074;
}

int GetSkipFrames()
{
    return m_skipFrames;
}

int SetSkipFrames(int skipFrames)
{
    return m_skipFrames = max(0, min(skipFrames, MAX_SKIP_FRAMES));
}

/** ResetSprites

    Go through all sprites and initialize everything SWOS forgot. :P
*/
static void ResetSprites()
{
    Sprite **currentSprite;
    for (currentSprite = allSpritesArray; *currentSprite != (Sprite *)-1; currentSprite++) {
        Sprite *s = *currentSprite;
        if (s->shirtNumber) {
            /* fraction part needs to be initialized to zero, or it will have a leftover value from previous game */
            s->x.setFraction(0);
            s->y.setFraction(0);
            s->ballDistance = 0;
            s->deltaX = s->deltaY = s->deltaZ = 0;
            s->playerDirection = 0;
            s->isMoving = 0;
            /* not sure what these are, but they gotta be synchronized too */
            s->unk009 = 0;
            s->unk010 = 0;
        }

        s->delayedFrameTimer = -1;
        s->frameIndex = -1;
        s->cycleFramesTimer = 1;
        s->direction = 0;
        s->startingDirection = 0;
        s->fullDirection = 0;
    }

    /* last, but not least, fix the ball itself */
    ballSprite->deltaX = ballSprite->deltaY = ballSprite->deltaZ = 0;
    *(word *)&ballSprite->x = 0;
    *(word *)&ballSprite->y = 0;
    *(word *)&ballSprite->z = 0;
}

/* player 2 on keyboard might have patched this, so we better save it */
static byte m_joy2SetStatusByte;

/** InitMultiplayerGame

    playerNo            -  0 = we are watcher, 1 = we are player one, 2 = we are player two
    inPlayerAddresses   -> array of player addresses
    numWatchers         -  number of watchers
    inWatchersAddresses -> array of watcher addresses
    player1Tactics      -> points to array of custom tactics for player 1 (from multiplayer options) - do not free
    player2Tactics      ->           -//-                               2

    Initialize multiplayer game related stuff and patch us in. Called before the game starts and
    after teams have been selected; before InitTeamsData(), when player presses Play Match
    (from inside SetupPlayers). Both input arrays are allocated as one memory block from the heap,
    and need to be deallocated when finished. Pair with FinishMultiplayerGame() to clean up properly.
*/
void InitMultiplayerGame(int playerNo, IPX_Address *playerAddresses, int numWatchers,
    const IPX_Address *inWatcherAddresses, const Tactics *pl1CustomTactics, const Tactics *pl2CustomTactics)
{
    assert(playerNo >= 0 && playerNo < 3);
    assert(g_joyKbdWord == 1 || g_joyKbdWord == 2); /* it should be clamped before entering setup team menus */
    m_burstPacketsSent = 0;
    m_currentFrameNo = 0;
    m_playerNo = playerNo;
    assert(playerNo >= 0 && playerNo <= 2);
    m_playerAddresses = playerAddresses;
    if (playerNo)
        InitTacticsContextSwitcher(pl1CustomTactics, pl2CustomTactics);
    m_numWatchers = numWatchers;
    assert(m_numWatchers >= 0 && m_numWatchers <= 6);
    m_watcherAddresses = inWatcherAddresses;
    memset(framesToRender, 0, sizeof(framesToRender));
    m_lastAppliedFrame = -1;
    teamSwitchCounter = 0;
    benchPickTeamTimer = 0;
    goalCounter = 0;
    stateGoal = 0;
    pl1BenchTimeoutCounter = pl2BenchTimeoutCounter = 0;

    /* not sure if we need these, but just in case */
    longFireFlag = longFireTime = longFireCounter = 0;

    /* we _definitely_ need this UGH $%*@(#$*! found it out the hard way... and it's only used ONCE */
    frameCount = 0;
    cameraXFraction = cameraYFraction = 0;  /* just in case... they don't seem to like clearing fractions */

    /* must reset these cause 2nd player controls might have something in them if pl2 on keyboard */
    pl1LastFired = pl2LastFired = 0;
    g_joy1Status = g_joy2Status = 0;
    pl1ShortFireCounter = pl2ShortFireCounter = 0;
    EGA_graphics = 0;

    /* gotta reset teams as there are some timers in there */
    memset((char *)&leftTeamData + 24, 0, sizeof(leftTeamData) - 24);
    memset((char *)&rightTeamData + 24, 0, sizeof(leftTeamData) - 24);

    paused = 0;
    m_sentThisFrame = false;
    m_mainLoopRan = 0;
    m_resendIndex = 0;
    memset(m_resendFrames, -1, sizeof(m_resendFrames));
#ifdef DEBUG
    memset(m_frameHashes, -1, sizeof(m_frameHashes));
    m_frameHashIndex = 0;
#endif
    m_sendLastFrame = false;
    m_gameStatus = GS_GAME_IN_PROGRESS;

    /* prevent SWOS from resetting joy2Status, so we can fake it in peace ;) */
    m_joy2SetStatusByte = *(byte *)Joy2SetStatus;
    *(byte *)Joy2SetStatus = 0xc3;
    m_savedNumLoopsJoy2 = g_numLoopsJoy2;
    g_numLoopsJoy2 = g_numLoopsJoy1;

    /*  set g_joyKbdWord: first player to whatever it was, 2nd player to joypad */
    g_joyKbdWord += 3;

    /* hook the main loop */
    PatchCall(main_loop, 1, HookMainLoop);

    if (m_numWatchers || !playerNo)
        InitWatchers(playerNo == 0);

    if (playerNo > 0) {
        m_otherPlayerAddress = &playerAddresses[(playerNo ^ 3) - 1];
    } else {
        /* skip engine calculations since we'll just render sprites from coordinates */
        *(byte *)((char *)GameLoop + 0x476) = 0xe9;
        *(dword *)((char *)GameLoop + 0x477) = 0x8f;    /* jump to UpdateStatistics, which we hooked */
    }

    /* wire up player 1 to locally controlled team, and player 2 to remotely controlled */
    if (playerNo == 1 || playerNo == 2) {
        leftTeamPlayerNo = playerNo;
        leftTeamCoachNo = 0;
        rightTeamPlayerNo = playerNo ^ 3;
        rightTeamCoachNo = 0;
        WriteToLog("leftTeamPlayerNo = %d, rightTeamPlayerNo = %d", leftTeamPlayerNo, rightTeamPlayerNo);
        /* prevent SWOS from resetting variables we just set */
        *(word *)((char *)SetupPlayers + 0x2cd) = 0x04eb;
        *(word *)((char *)SetupPlayers + 0x45f) = 0x04eb;
        m_nextSendFrameNo = playerNo == 2;      /* left player starts with 0, right with frame 1 */
    }

    /* This is a big synchronization problem, stoppageTimer is incremented from interrupt handler
       that is hardware triggered, and value is later used in lot of calculations about the ball
       and stuff. That has an effect that value changes in a kind of unpredictable way. What we're
       gonna do here is patch interrupt handler to prevent increasing it, then bump it manually
       each frame. Game frames and timer are approximately equal, and what consequences this will
       have on the game itself remains to be seen. */
    *(word *)((char *)TimerProc + 0x24) = 0x05eb;

    /* g_currentTick is used in calculation which player wins the ball, so we will replace it
       with stoppageTimer */
    memcpy((char *)CalculateIfPlayerWinsBall + 0x30d, (char *)GameLoop + 0x592, 4);

    /* When statistics are shown on the screen it is done within a busy loop,
       so we gotta hook that to keep the clients from timing out */
    *(dword *)((char *)GameLoop + 0x3e8) = (dword)ShowStatsLoop - (dword)GameLoop - 0x3e7 - 5;

    /* Hook paused loop too. We will take it over completely. */
    *(dword *)((char *)GameLoop + 0x3a1) = (dword)PausedLoop - (dword)GameLoop - 0x3a0 - 5;
    *(word *)((char *)GameLoop + 0x3a5) = 0xeeeb;       /* jmp paused_loop */

    /* Disable exiting stats by firing, since it's scanned after we send the frame. Should find
       a better solution to this sometime. */
    PatchByte(UpdateStatistics, 0x36, 0xc3);

    /* Disable stat_fire_exit in UpdateAndApplyTeamControls (doesn't increment teamSwitchCounter then) */
    *(byte *)((char *)UpdateAndApplyTeamControls + 0xe) = 0xeb;
    *(dword *)((char *)GameLoop + 0x5b0) = (dword)GameEnded - (dword)GameLoop - 0x5af - 5;

    /* invoke FinishMultiplayerGame when exiting game loop, overwrite call to replay/continue menu */
    PatchCall(GameLoop, 0x663, FinishMultiplayerGame);

    /* insert additional network poll right before flip */
    PatchByte(Flip, 0, 0xe8);
    PatchCall(Flip, 1, HookFlip);
    PatchDword(Flip, 5, 0x0fffc883);            /* this makes EGA_graphics test never succeed */
    PatchWord(SWOS_ReadGamePort, 7, 0x7505);    /* don't check joypad if player is on keyboard */
    PatchCall(GameLoop, 0x50b, HookUpdateStatistics);

    /* hook calls to GetPlayerAtIndex to prevent it return player from wrong team
       (if not patched, we get same team twice if both players picked same team) */
    PatchCall(GetPlayerByOrdinal, 0xd4, HookGetPlayerAtIndex);
    PatchCall(InitIngameTeamStructure, 0x1bc, HookGetPlayerAtIndex);

    /* insert network polling during fades since if we're the first player, player 2 might
       be waiting on us to acknowledge their key packets */
    HookNetworkPoll();

    /* turn off 2nd player on keyboard if active, and save us _A LOT_ of trouble
       (for instance - it sets joy2Status directly from keyboard handler) */
    if ((m_pl2KeyboardWasActive = pl2Keyboard))
        SetSecondPlayerOnKeyboard(false);

    DisableInput();     /* start with disabled input, only enable when needed */
    ResetSprites();     /* SWOS doesn't initialize sprites properly... it just wasn't manifesting before this */
    WriteToLog("Main loop patched and hooks installed, g_joyKbdWord set to to %hd.", g_joyKbdWord);
    WriteToLog("Initializing game... Player addresses:");
    WriteToLog("Player1: [%#.12s]", &playerAddresses[0]);
    WriteToLog("Player2: [%#.12s]", &playerAddresses[1]);
    WriteToLog("Our player number is %d", playerNo);
    WriteToLog("Number of watchers: %d", numWatchers);
    WriteToLog("Skip frames: %d", m_skipFrames);
}

/** FinishMultiplayerGame

    Clean up after the game and unpatch.
*/
extern "C" void FinishMultiplayerGame()
{
    WriteToLog("Finishing multiplayer game...");
    qFree(m_playerAddresses);
    m_playerAddresses = nullptr;
    CleanupWatchers();
    if (m_playerNo)
        DisposeTacticsContextSwitcher();

    g_numLoopsJoy2 = m_savedNumLoopsJoy2;
    if (g_joyKbdWord != 4 && g_joyKbdWord != 5)
        WriteToLog("Warning, g_joyKbdWord = %d in FinishMultiplayerGame()", g_joyKbdWord);

    g_joyKbdWord -= 3;
    PatchByte(Joy2SetStatus, 0, m_joy2SetStatusByte);
    PatchDword(main_loop, 1, 0x00007b32);
    memcpy(Player1StatusProc, VerifyJoypadControls, 8);
    memcpy(Player2StatusProc, (char *)SWOS_ReadGamePort + 0x1b, 8);
    PatchWord(SetupPlayers, 0x2cd, 0xa366);
    PatchWord(SetupPlayers, 0x45f, 0xa366);
    PatchWord(TimerProc, 0x24, 0xff66);
    memcpy((char *)CalculateIfPlayerWinsBall + 0x30d, (char *)TimerProc + 0xe, 4);
    PatchDword(GameLoop, 0x3e8, 0x000002c5);
    PatchDword(GameLoop, 0x3a1, 0x000048fc);
    PatchWord(GameLoop, 0x3a5, 0xbfe8);
    PatchByte(UpdateStatistics, 0x36, 0xe8);
    PatchDword(InitIngameTeamStructure, 0x1bc, 0xffff4a95);
    PatchDword(GetPlayerByOrdinal, 0xd4, 0xfffd1b73);
    UnhookNetworkPoll();
    PatchByte(UpdateAndApplyTeamControls, 0xe, 0x74);
    PatchDword(GameLoop, 0x663, 0x00000994);
    PatchDword(GameLoop, 0x5b0, 0x000851f6);
    /* use memcpy to avoid GCC warning about breaking strict-aliasing rules */
    dword d = 0xc93d8366;
    memcpy(Flip, &d, sizeof(dword));
    PatchDword(Flip, 0x03, &EGA_graphics);
    PatchByte(Flip, 0x07, 0xff);
    PatchWord(SWOS_ReadGamePort, 0x7, 0x7401);
    PatchDword(GameLoop, 0x50b, 0x000522f5);
    PatchByte(GameLoop, 0x476, 0xe8);
    PatchDword(GameLoop, 0x477, 0x05e4fd);
    EnableInput();

    if (m_pl2KeyboardWasActive)
        SetSecondPlayerOnKeyboard(true);
}

/** OnGameLoopStart

    Executed before the main game loop starts. Keep polling network until we
    receive other player controls for this frame.
*/
void OnGameLoopStart()
{
    word startTime = g_currentTick, networkTimeout = GetNetworkTimeout();
    int currentResendIndex = m_resendIndex;
#if 0
    /* very nice for desync resolving :P */
    static int gameNo;
    extern dword swosDataBase;
    if (!m_currentFrameNo) {
        if (gameNo == 1) {
            HexDumpToLog(&swosDataBase, 0xc5fc0, "entire data");
            HexDumpToLog(&m_currentFrameNo, (dword)&m_currentFrameNo - (dword)&m_skipFrames + 1, "mpgame.c data");
        }

        gameNo++;
        startTime = currentTick;
        networkTimeout = 20000;
    }
#endif

    if (!m_currentFrameNo && !m_playerNo) {
        /* set first packet to whatever we've got right now - we're in sync for the first frame */
        int packetSize;
        CreateWatcherPacket(&packetSize, m_currentFrameNo);
    }

    WriteToLog(LM_GAME_LOOP, "==================================================");
    WriteToLog(LM_GAME_LOOP, "OnGameLoopStart(), m_currentFrameNo = %d, currentTick = %d, teamPlayingUp = %d, "
        "paused = %d, showingStats = %d, teamSwitchCounter = %d, stoppageTimer = %d", m_currentFrameNo, g_currentTick,
        teamPlayingUp, paused, showingStats, teamSwitchCounter, stoppageTimer);
    HexDumpToLog(LM_GAME_LOOP_STATE, &leftTeamData, 145, "left team");
    HexDumpToLog(LM_GAME_LOOP_STATE, &rightTeamData, 145, "right team");

    /* watchers don't need to wait or send anything, just try to empty out the packet queue here,
       but also check if we got disconnected from the game */
    if (!m_playerNo) {
        for (int i = 0; i < 20; i++)
            if (!ReceiveFrame())
                break;

        if (WatcherTimeout(networkTimeout)) {
            /* we are disconnected! */
            WriteToLog("Watcher timeout expired, aborting!");
            gameState = GS_GAME_DISCONNECTED;
            paused = showingStats = showStats = playGame = false;
        }

        return;
    }

    StoreCurrentHash();

    /* cycle starts all over again */
    m_sentThisFrame = false;
    SendNextFrame();

    if (framesToRender[0].type != PT_GAME_CONTROLS)
        WriteToLog(LM_GAME_LOOP, "Waiting for packet %d", m_currentFrameNo);

    /* wait till we have next frame to render, or until time is up */
    while (startTime + networkTimeout > g_currentTick) {
        if (framesToRender[0].type == PT_GAME_CONTROLS) {
            WriteToLog(LM_GAME_LOOP, "Found next frame to render (%d)... controls = %#x, state = %#x",
                framesToRender[0].frameNo, framesToRender[0].controls, framesToRender[0].state);
            /* do not apply same frame twice */
            if (m_currentFrameNo != m_lastAppliedFrame) {
                ApplyFrame(framesToRender);
                m_lastAppliedFrame = m_currentFrameNo;
            }

            if (g_currentTick - startTime)
                WriteToLog(LM_GAME_LOOP, "Wasted %d tick(s)", g_currentTick - startTime);

            return;
        }

        ReceiveFrame();
        if (!playGame) {
            WriteToLog("Exit game request received. Exiting packet wait loop.");
            return;
        }

        //calla_ebp_safe(GetKey);  /* allow interrupt */
        //testiraj alt-f1 nekako eksplicitno
        //nacrtaj pescanik il nesto (moze cak i animirani)
        /* resend frames from newest to oldest */
        if (m_currentFrameNo > 0) {
            /* seems we didn't find what we were looking for, resend last packet
               as it may have been lost - but try not to flood, send certain number
               of packets as fast as possible, then slow down */
            assert(currentResendIndex >= 0 && currentResendIndex < (int)sizeofarray(m_resendFrames));
            if (m_burstPacketsSent < MAX_BURST_PACKETS) {
                ResendFrame *rs = getPreviousResendFrame(&currentResendIndex);
                if (rs->frameNo >= 0) {
                    SendFrame(rs->frameNo, rs->controls, rs->state);
                    m_burstPacketsSent++;
                }
            } else if (m_lastSendTime + SLOW_SEND_TICKS < g_currentTick) {
                ResendFrame *rs = getPreviousResendFrame(&currentResendIndex);
                if (rs->frameNo >= 0) {
                    SendFrame(rs->frameNo, rs->controls, rs->state);
                    m_lastSendTime = g_currentTick;
                }
            }
        }
        IPX_OnIdle();
    }

    /* timeout waiting on next frame; we're not communicating with other player anymore */
    m_gameStatus = GS_GAME_DISCONNECTED;
    WriteToLog("Game loop timed out while waiting for frame %d!", m_currentFrameNo);
    DumpSavedStates();
    /* turn off everything so we jump right out of the loop */
    paused = showingStats = playGame = false;
}

void OnGameLoopEnd()
{
    WriteToLog(LM_GAME_LOOP, "OnGameLoopEnd()");
    m_burstPacketsSent = 0;
    m_currentFrameNo++;
    stoppageTimer += !paused;   /* do not increment while paused */
    /* shift frames to render, get rid of the one we just rendered */
    memmove(framesToRender, framesToRender + 1, sizeof(framesToRender) - sizeof(framesToRender[0]));
    framesToRender[sizeofarray(framesToRender) - 1].type = PT_GAME_CONTROLS + 1;
    WriteToLog(LM_GAME_LOOP_STATE, "Game loop end, currentFrame = %d", m_currentFrameNo);
    HexDumpToLog(LM_GAME_LOOP_STATE, &leftTeamData, 145, "left team");
    HexDumpToLog(LM_GAME_LOOP_STATE, &rightTeamData, 145, "right team");
}

byte GetGameStatus()
{
    return m_gameStatus;
}

/* Take over stats loop. */
void ShowStatsLoop()
{
    while (showingStats) {
        int teamControls;
        WriteToLog(LM_GAME_LOOP, "ShowStatsLoop(), showingStats = %d, teamSwitchCounter = %d",
            showingStats, teamSwitchCounter);
        OnGameLoopStart();
        calla_ebp_safe(GetKey);
        calla_ebp_safe(MainKeysCheck);
        calla_ebp_safe(ClearBackground);
        calla(UpdateStatistics);
        if (!showingStats)
            break;
        teamSwitchCounter++;
        teamControls = GetControlScanPlayerNumber();
        if (teamControls == 1)
            calla_ebp_safe(Player1StatusProc);
        else if (teamControls == 2)
            calla_ebp_safe(Player2StatusProc);
        HookUpdateStatistics();  /* to avoid watchers timing out */
        WriteToLog(LM_GAME_LOOP, "showingStats = %d", showingStats);
        OnGameLoopEnd();
    }
}

/** PausedLoop

    This will be executing in a loop while the game is paused. It can be entered only from
    single place at the beginning, setting paused to true will cause next frame to be paused.
    However unsetting it early will resume game in the same frame.
*/
void PausedLoop()
{
    int teamControls;
    WriteToLog(LM_GAME_LOOP, "PausedLoop(), paused = %d", paused);
    OnGameLoopStart();

    if (!paused)
        return;

    calla_ebp_safe(GetKey);
    calla_ebp_safe(MainKeysCheck);

    if (!paused)
        return;

    teamSwitchCounter++;
    teamControls = GetControlScanPlayerNumber();

    if (teamControls == 1)
        calla_ebp_safe(Player1StatusProc);
    else if (teamControls == 2)
        calla_ebp_safe(Player2StatusProc);

    HookUpdateStatistics();  /* to avoid watchers timing out */
    OnGameLoopEnd();
}

/** GetRandomVariables

    vars -> buffer for random variables values
    size -> on entry size of input buffer, on exit actual size of random variables data

    Collect values of random variables to given buffer. seed2 vars and random_seed are
    overkill - they don't seem to be used during the game loop, but it can't hurt.
*/
void GetRandomVariables(byte *vars, int *size)
{
    assert(*size >= 8);
    *size = 8;
    memcpy(vars, &seed, 3);
    memcpy(vars + 3, &seed2, 3);
    *(word *)(vars + 6) = randomSeed;
}

void SetRandomVariables(const char *vars, int size)
{
    assert(size >= 8);
    if (size >= 8) {
        memcpy(&seed, vars, 3);
        memcpy(&seed2, vars + 3, 3);
        randomSeed = *(word *)(vars + 6);
    }
}
