/** mpcontrol.cpp

    Implement player controls during play match menu; one player at a time is in control of
    cursor while setting up their team, and their controls are sent over to other player and
    all of the potential watchers.
*/

#include "mpcontrol.h"
#include "mplobby.h"
#include "mppacket.h"
#include "mplayer.h"
#include "qalloc.h"

/* keep track of commands received from the network */
static dword m_networkControlQueue[KEY_BUFFER_SIZE];
static signed char m_numCommands;

/* last time we've seen a packet from the controlling player, or sent one if we're controlling */
static word m_lastPacketTime = -1;

/* tick when we started waiting for other player to confirm keys */
static word m_waitingStartTick;

static void (*m_startTheGameFunction)();

static void InitializeTheGame();

/** BroadcastControls

    controls     - keys, bit coded (controlWord or joy1Status)
    longFireFlag - evident

    Broadcast our controls to all connected players if we are currently controlling player.
    Update last packet time. Called from CheckControls when there's a keypress.
*/
void BroadcastControls(byte controls, word longFireFlag)
{
    static byte lastControls;

    if (IsOurPlayerControlling() && (controls || lastControls)) {
        int length;
        char *packet = CreateControlsPacket(&length, controls, longFireFlag);

        for (int i = 0; i < GetNumPlayers(); i++)
            if (i != GetOurPlayerIndex())
                SendImportantPacket(&GetPlayer(i)->address, packet, length);

        m_lastPacketTime = g_currentTick;
    }
}

/** AddControlsToNetworkQueue

    controls     - SWOS' bitfield of controls (i.e. joy1Status)
    longFireFlag - SWOS' long fire flag

    Store controls for this frame to network keys buffer for sending later.
    If it's full controls will be lost.
*/
static void AddControlsToNetworkQueue(byte controls, word longFireFlag)
{
    if (m_numCommands < (int)sizeof(m_networkControlQueue))
        m_networkControlQueue[m_numCommands++] = controls | (longFireFlag << 16);
    else
        WriteToLog("Command buffer full. Lost a command.");
}

/* Return -1 to signal we should get the real key. */
dword GetControlsFromNetwork()
{
    if (IsOurPlayerControlling())
        return -1;

    return m_numCommands > 0 ? m_networkControlQueue[--m_numCommands] : 0;
}

void InitializeNetworkKeys()
{
    /* if we're controlling player send ping packet ASAP */
    m_lastPacketTime = IsOurPlayerControlling() ? 0 : g_currentTick;
    m_numCommands = 0;
}

/** HandleNetworkKeys

    Called from NetworkOnIdle(). Handles network communication while players are
    setting up their teams.
*/
void HandleNetworkKeys()
{
    int length;
    IPX_Address node;
    byte controls;
    word longFireFlag;

    char *packet = ReceivePacket(&length, &node);

    if (!IsOurPlayerControlling()) {
        if (packet) {
            if (!AddressMatch(&node, GetControllingPlayerAddress())) {
                WriteToLog("Someone else is trying to send the keys?");
                qFree(packet);
                return;
            }

            switch (GetRequestType(packet, length)) {
            case PT_CONTROLS:
                if (UnpackControlsPacket(packet, length, &controls, &longFireFlag)) {
                    AddControlsToNetworkQueue(controls, longFireFlag);
                    m_lastPacketTime = g_currentTick;
                } else
                    WriteToLog("Invalid scan code packet ignored.");
                break;

            case PT_PING:
                m_lastPacketTime = g_currentTick;
                break;
            }
        }

        /* check when was the last packet from controlling player, did we lose them? */
        if (g_currentTick - m_lastPacketTime > GetNetworkTimeout())
            ControllingPlayerTimedOut();
    } else {
        /* ignore incoming packets, but check if we are too idle and need to send ping;
           we'll send ping at 3/4 of network timeout to allow some slack for packet to arrive
           so clients wouldn't disconnect us prematurely */
        if (g_currentTick - m_lastPacketTime > 3 * GetNetworkTimeout() / 4) {
            word packetType = PT_PING;
            auto packet = (char *)&packetType;
            int length = sizeof(word);

            for (int i = 0; i < GetNumPlayers(); i++)
                if (i != GetOurPlayerIndex())
                    SendImportantPacket(&GetPlayer(i)->address, packet, length);

            m_lastPacketTime = g_currentTick;
        }
    }

    qFree(packet);
}

/** SaveTeamChanges

    Any change done to lineup in play match menu will be reflected on teams stored in selectedTeams.
    That's why find our team in there by id and copy over anything that player changed.
*/
static void SaveTeamChanges()
{
    auto playerTeamIndex = GetCurrentPlayerTeamIndex();
    assert(playerTeamIndex == 0 || playerTeamIndex == 1);
    StoreTeamData(&g_selectedTeams[playerTeamIndex]);
}

/** SwitchToNextControllingState

    startTheGameFunction -> call this when ready to rumble ;)
                            (this is what happens when play match is pressed)

    Called after a player finishes setting up the team by pressing play match.
    Return kWait to signal that we will be waiting for pending packets to come through.
    Return kGoPlayer1 to give control to second player, and kGoPlayer2 to continue
    into the game.
*/
int SwitchToNextControllingState(void (*startTheGameFunction)())
{
    m_startTheGameFunction = startTheGameFunction;

    if (GetState() == ST_PL1_SETUP_TEAM) {
        WriteToLog(LM_SYNC, "Player 2 now setting up the team.");
        SwitchToState(ST_PL2_SETUP_TEAM);

        auto ourPlayer = GetPlayer(GetOurPlayerIndex());
        if (ourPlayer->isPlayer() && ourPlayer->isControlling())
            SaveTeamChanges();

        /* player 1 finished, pass on control to player 2 */
        int i;
        for (i = 0; i < GetNumPlayers(); i++) {
            auto player = GetPlayer(i);
            if (player->isPlayer()) {
                if (player->isControlling()) {
                    WriteToLog("Player %d not controlling anymore.", i);
                    player->resetControlling();
                } else {
                    player->setControlling();
                    WriteToLog("Player %d now in control.", i);
                    break;
                }
            }
        }

        assert(i < GetNumPlayers());
        InitializeNetworkKeys();

        /* make current controlling player tactics show in all clients */
        assert(GetPlayer(i)->userTactics);
        memcpy(USER_A, GetPlayer(i)->userTactics, sizeof(Tactics) * 6);
        return PlayMatchMenuCommand::kGoPlayer1;
    } else if (GetState() == ST_PL2_SETUP_TEAM) {
        if (IsOurPlayerControlling()) {
            /* if we are the controlling player do not start the game until all the keys got
               through, since once we enter the game key packet sending will cease, and others
               might be stuck without the final part (one that actually starts the game!) */
            SwitchToState(ST_CONFIRMING_KEYS);
            m_waitingStartTick = g_currentTick;
            return PlayMatchMenuCommand::kWait;
        } else {
            /* we can let this guy begin immediately */
            InitializeTheGame();
            return PlayMatchMenuCommand::kGoPlayer2;
        }
    } else
        WriteToLog("Invalid state %s!!!", StateToString(GetState()));

    return false;
}

static void InitializeTheGame()
{
    /* game about to go! */
    int playerNo = 0, numWatchers = 0, playerIndex = 0;
    IPX_Address *addresses;
    const IPX_Address *watcherAddresses[MAX_PLAYERS - 2];
    const IPX_Address *playerAddresses[2];
    const Tactics *pl1CustomTactics = nullptr, *pl2CustomTactics = nullptr;

    /* player 2 finished, starting the game! */
    WriteToLog("Game is starting!");
    SwitchToState(ST_GAME);

    auto ourPlayerIndex = GetOurPlayerIndex();
    for (int i = 0; i < GetNumPlayers(); i++) {
        auto player = GetPlayer(i);
        if (player->isPlayer()) {
            playerAddresses[playerIndex++] = player->getAddress();
            assert(playerIndex <= 2);
            if (i == ourPlayerIndex) {
                playerNo = playerIndex;
                if (player->isControlling())
                    SaveTeamChanges();
            }
            *(!pl1CustomTactics ? &pl1CustomTactics : &pl2CustomTactics) = player->getTactics();
        } else
            watcherAddresses[numWatchers++] = player->getAddress();

        player->setUnsynced();  /* clear synced flag in case there's a leftover from previous game */
    }

    addresses = (IPX_Address *)qAlloc((numWatchers + 2) * sizeof(IPX_Address));
    assert(addresses);
    memcpy(addresses, playerAddresses[0], sizeof(IPX_Address));
    memcpy(addresses + 1, playerAddresses[1], sizeof(IPX_Address));

    for (int i = 0; i < numWatchers; i++)
        memcpy(addresses + i + 2, watcherAddresses[i], sizeof(IPX_Address));

    /* usually the first packet will get there, so save other player from having
       to wait through the fades to get their final key packets acknowledged */
    DismissIncomingPackets();

    assert(pl1CustomTactics && pl2CustomTactics && m_startTheGameFunction);
    InitMultiplayerGame(playerNo, addresses, numWatchers, addresses + 2, pl1CustomTactics, pl2CustomTactics);
    m_startTheGameFunction();
}

static bool ModalTimeoutElapsed()
{
    const int kModalDialogDelay = 10;   /* only display modal dialog if waiting longer than this (in ticks) */
    return g_currentTick - m_waitingStartTick > kModalDialogDelay;
}

/** WaitForKeysConfirmation

    Handle wait for key confirmation state. Wait until all the pending packets
    have been confirmed, and only then start the game. Return false if we're still
    staying in this mode so menu drawing is blocked from overwriting our modal dialog.
*/
bool WaitForKeysConfirmation(SetupTeamsModalFunction modalFunction)
{
    /* it'll take care of ack packets and rid us of our pending packets */
    DismissIncomingPackets();

    if (!UnacknowledgedPacketsPending()) {
        WriteToLog("Other player acknowledged all the key packets, starting the game");
        InitializeTheGame();
    } else {
        if (ModalTimeoutElapsed()) {
            assert(modalFunction);
            modalFunction();
        }
    }

    return false;
}
