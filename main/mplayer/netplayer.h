#pragma once

#include "dosipx.h"

#define NICKNAME_LEN    20      /* maximum multiplayer nickname length */
#define GAME_NAME_LENGTH NICKNAME_LEN + 10

struct NetPlayer {
    char name[NICKNAME_LEN + 1];
    TeamFile team;
    Tactics *userTactics;   /* user tactics, allocated when/if necessary */
    IPX_Address address;
    byte flags;             /* bit 0 - is watcher
                               bit 1 - is ready
                               bit 2 - is synced
                               bit 3 - is player in hold of menu controls */
    word joinId;
    word lastPingTime;

    struct PlayerFlags {
        enum {
            isWatcher      = 1,
            isReady        = 2,
            isSynced       = 4,
            isControlling  = 8,
        };
    };

    char *getName() {
        return name;
    }

    const IPX_Address *getAddress() const {
        return &address;
    }

    const Tactics *getTactics() const {
        return userTactics;
    }

    bool isWatcher() const {
        return flags & PlayerFlags::isWatcher;
    }

    bool isPlayer() const {
        return !isWatcher();
    }

    bool isReady() const {
        return flags & PlayerFlags::isReady;
    }

    bool isControlling() const {
        return flags & PlayerFlags::isControlling;
    }

    bool isSynced() const {
        return flags & PlayerFlags::isSynced;
    }

    void setSynced() {
        flags |= PlayerFlags::isSynced;
    }

    void setUnsynced() {
        flags &= ~PlayerFlags::isSynced;
    }

    void setControlling() {
        flags |= PlayerFlags::isControlling;
    }

    void resetControlling() {
        flags &= ~PlayerFlags::isControlling;
    }
};
