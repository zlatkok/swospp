/**

    mptact.h

    Implementation of user tactics for both players at the same time.

*/

#pragma once

void InitTacticsContextSwitcher(const Tactics *player1CustomTactics, const Tactics *player2CustomTactics);
void DisposeTacticsContextSwitcher();
