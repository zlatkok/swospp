#pragma once

#include "netplayer.h"
#include "mplobby.h"

struct PlayMatchMenuCommand {
    enum {
        kWait = 0,
        kGoPlayer1 = 1,
        kGoPlayer2 = 2,
    };
};

void HandleNetworkKeys();
void InitializeNetworkKeys();
bool WaitForKeysConfirmation(SetupTeamsModalFunction modalFunction);

extern "C" {
    void BroadcastControls(byte controls, word longFireFlag);
    dword GetControlsFromNetwork();
    int SwitchToNextControllingState(void (*startTheGameFunction)());
}
