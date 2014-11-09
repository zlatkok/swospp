/***

   SWOS++ Multiplayer packet handling routines
   ===========================================

***/

#include "types.h"
#include "util.h"
#include "dosipx.h"
#include "mplayer.h"
#include "mppacket.h"

static char packet[3 * 1024];

word getRequestType(const char *packet, int length)
{
    if (length < sizeof(word))
        return PT_NONE;
    return *(word *)packet;
}

char *createGetGameInfoPacket(int *length)
{
    *(word *)packet = PT_GET_GAME_INFO;
    *length = sizeof(word);
    return packet;
}

char *createGameInfoPacket(int *length, int numPlayers, char *gameName, byte id)
{
    char *p;
    word *w = (word *)packet;
    w[0] = PT_GAME_INFO_DATA;
    w[1] = NETWORK_VERSION;
    w[2] = NETWORK_SUBVERSION;
    w[3] = numPlayers & 255 | MAX_PLAYERS << 8;
    *(p = strcpy(packet + 8, gameName)) = id;
    *length = p - packet + 1;
    assert(*length <= sizeof(packet));
    return packet;
}

bool unpackGameInfoPacket(const char *data, int length, int *networkVersion, int *networkSubversion,
    int *numPlayers, int *maxPlayers, char *gameName, int maxGameNameLen, byte *id)
{
    word *w = (word *)data;
    assert(*w == PT_GAME_INFO_DATA);
    if (length < 9)
        return false;
    *networkVersion = w[1];
    *networkSubversion = w[2];
    *numPlayers = w[3] & 255;
    *maxPlayers = w[3] >> 8;
    if (*numPlayers > 99 || *maxPlayers > 99)
        return false;
    *(char *)memcpy(gameName, data + 8, min(maxGameNameLen - 1, length - 8 - 1)) = '\0';
    *id = data[length - 1];
    return true;
}

char *createJoinGamePacket(int *length, byte joinId, byte gameId, const char *name, const char *teamName)
{
    word *w = (word *)packet;
    w[0] = PT_JOIN_GAME;
    w[1] = NETWORK_VERSION;
    w[2] = NETWORK_SUBVERSION;
    packet[6] = joinId;
    packet[7] = gameId;
    *length = strcpy(strcpy(packet + 8, name) + 1, teamName) - packet;
    assert(*length <= sizeof(packet));
    return packet;
}

bool unpackJoinGamePacket(const char *data, int length, word *networkVersion,
    word *networkSubversion, byte *joinId, byte *gameId, char *name, char *teamName)
{
    const char *p = data + 8, *limit = data + 8 + min(NICKNAME_LEN + 1, length - 8);
    word *w = (word *)data;
    if (length < 8)
        return false;
    assert(w[0] == PT_JOIN_GAME);
    *networkVersion = w[1];
    *networkSubversion = w[2];
    *joinId = data[6];
    *gameId = data[7];
    while (p < limit && *p)
        *name++ = *p++;
    if (p >= limit)
        return false;
    *name = *p++;
    limit  = p + min(MAX_TEAM_NAME_LEN + 1, length - (p - data));
    while (p < limit && *p)
        *teamName++ = *p++;
    *teamName = '\0';
    return true;
}

char *createRefuseJoinGamePacket(int *length, byte errorCode)
{
    *(word *)packet = PT_JOIN_GAME_REFUSED;
    packet[2] = errorCode;
    assert(errorCode != GR_OK);
    *length = 3;
    return packet;
}

bool unpackRefuseJoinGamePacket(const char *data, int length, byte *errorCode)
{
    if (length < 3)
        return false;
    *errorCode = data[2];
    return true;
}

char *createJoinedGameOkPacket(int *length, const LobbyState *state, const IPX_Address **addresses)
{
    int i;
    char *p = packet;
    *(word *)p = PT_JOIN_GAME_ACCEPTED;
    p[2] = state->numPlayers;
    for (p += 3, i = 0; i < state->numPlayers; i++) {
        *(p = strncpy(strcpy(p, state->playerNames[i]) + 1, state->playerTeamsNames[i], MAX_TEAM_NAME_LEN)) = '\0';
        *++p = state->playerFlags[i];
        memcpy(p + 1, addresses[i], sizeof(IPX_Address));
        p += sizeof(IPX_Address) + 1;
    }
    memcpy(p, &state->options, sizeof(MP_Options));
    *length = p - packet + sizeof(MP_Options);
    assert(*length <= sizeof(packet));
    return packet;
}

bool unpackJoinedGameOkPacket(const char *data, int length, LobbyState *state, IPX_Address **addresses)
{
    int i;
    const char *p, *limit;
    char *q;
    if (length < 3 + 3 + sizeof(IPX_Address) + sizeof(MP_Options))
        return false;
    p = data;
    assert(*(word *)data == PT_JOIN_GAME_ACCEPTED);
    if ((state->numPlayers = data[2]) > MAX_PLAYERS)
        return false;
    for (p += 3, i = 0; i < state->numPlayers; i++) {
        /* player nickname */
        q = state->playerNames[i];
        assert(q);
        limit = p + min(NICKNAME_LEN + 1, length - (p - data));
        while (p < limit && *p)
            *q++ = *p++;
        if (p >= limit)
            return false;
        *q = *p++;
        /* player team name */
        q = state->playerTeamsNames[i];
        assert(q);
        limit = p + min(MAX_TEAM_NAME_LEN + 1, length - (p - data));
        while (p < limit && *p)
            *q++ = *p++;
        if (p >= limit)
            return false;
        *q = *p++;
        /* player flags */
        limit = data + length;
        if (p >= limit)
            return false;
        state->playerFlags[i] = *p++;
        /* player address */
        if (p + sizeof(IPX_Address) <= limit) {
            copyAddress(addresses[i], (IPX_Address *)p);
            p += sizeof(IPX_Address);
        } else
            return false;
    }
    if (p + sizeof(MP_Options) > limit)
        return false;
    memcpy(&state->options, p, sizeof(MP_Options));
    return true;
}

char *createOptionsPacket(int *length, const MP_Options *options)
{
    assert(sizeof(packet) >= sizeof(MP_Options) + sizeof(word));
    *(word *)packet = PT_OPTIONS;
    memcpy(packet + 2, options, sizeof(MP_Options));
    *length = sizeof(MP_Options) + sizeof(word);
    return packet;
}

bool unpackOptionsPacket(const char *data, int length, MP_Options *options)
{
    assert(*(word *)data == PT_OPTIONS);
    if (length != sizeof(MP_Options) + sizeof(word))
        return false;
    memcpy(options, data + 2, sizeof(MP_Options));
    return true;
}

char *createPlayerJoinedPacket(int *length, const char *playerName, const char *playerTeamName)
{
    *(word *)packet = PT_PLAYER_JOINED;
    *length = strcpy(strcpy(packet + 2, playerName) + 1, playerTeamName) - packet;
    assert(*length + 1 <= sizeof(packet));
    return packet;
}

bool unpackPlayerJoinedPacket(const char *data, int length, char *playerName, char *playerTeamName)
{
    const char *p = data + 2, *limit = data + 2 + min(NICKNAME_LEN + 1, length - 2);
    assert(*(word *)data == PT_PLAYER_JOINED);
    if (length < 3)
        return false;
    while (p < limit && *p)
        *playerName++ = *p++;
    if (p >= limit)
        return false;
    *playerName = *p++;
    limit = p + min(MAX_TEAM_NAME_LEN + 1, length - (p - data));
    while (p < limit && *p)
        *playerTeamName++ = *p++;
    *playerTeamName = '\0';
    return true;
}

char *createPlayerFlagsPacket(int *length, int flags, int playerIndex)
{
    *(word *)packet = PT_PLAYER_FLAGS;
    packet[2] = playerIndex;
    packet[3] = flags;
    *length = 4;
    assert(*length <= sizeof(packet));
    return packet;
}

bool unpackPlayerFlagsPacket(const char *data, int length, int *playerIndex, int *flags)
{
    assert(*(word *)data == PT_PLAYER_FLAGS);
    if (length != 4)
        return false;
    *playerIndex = data[2];
    *flags = data[3];
    return true;
}

char *createPlayerTeamChangePacket(int *length, int playerIndex, char *teamName)
{
    *(word *)packet = PT_PLAYER_TEAM;
    packet[2] = playerIndex;
    *length = strcpy(packet + 3, teamName) - packet;
    assert(*length <= sizeof(packet));
    return packet;
}

bool unpackPlayerTeamChangePacket(const char *data, int length, int *playerIndex, char *teamName)
{
    const char *p = data + 3;
    assert(*(word *)data == PT_PLAYER_TEAM);
    if (length < 4)
        return false;
    *playerIndex = data[2];
    while (p < data + length && *p)
        *teamName++ = *p++;
    *teamName = '\0';
    return true;
}

void setPlayerIndex(char *data, int playerIndex)
{
    assert(*(word *)data == PT_PLAYER_TEAM || *(word *)data == PT_PLAYER_FLAGS ||
        *(word *)data == PT_PLAYER_LEFT);
    data[2] = playerIndex;
}

char *createPlayerLeftPacket(int *length, int playerIndex)
{
    *(word *)packet = PT_PLAYER_LEFT;
    packet[2] = playerIndex;
    *length = 3;
    return packet;
}

bool unpackPlayerLeftPacket(const char *data, int length, int *playerIndex)
{
    assert(*(word *)data == PT_PLAYER_LEFT);
    if (length != 3)
        return false;
    *playerIndex = data[2];
    return true;
}

char *createPlayerChatPacket(int *length, const char *text, byte color)
{
    *(word *)packet = PT_CHAT_LINE;
    packet[2] = color;
    *length = strncpy(packet + 3, text, MAX_CHAT_LINE_LENGTH) - packet;
    assert(*length <= sizeof(packet));
    return packet;
}

bool unpackPlayerChatPacket(const char *data, int length, char *text, byte *color)
{
    assert(*(word *)data == PT_CHAT_LINE);
    if (length > MAX_CHAT_LINE_LENGTH + sizeof(word) + 1 || length < sizeof(word) + 1)
        return false;
    *color = data[2];
    *strncpy(text, data + 3, min(MAX_CHAT_LINE_LENGTH, length - sizeof(word) - 1)) = '\0';
    return true;
}

char *createSyncPacket(int *length, const IPX_Address **addresses, int numPlayers,
    const byte *randomVars, int randomVarsLength)
{
    int i;
    char *p = packet + 2;
    *(word *)packet = PT_SYNC;
    for (i = 0; i < numPlayers; i++) {
        memcpy(p, addresses[i], sizeof(IPX_Address));
        p += sizeof(IPX_Address);
    }
    memcpy(p, randomVars, randomVarsLength);
    *length = p + randomVarsLength - packet;
    assert(*length <= sizeof(packet));
    return packet;
}

bool unpackSyncPacket(const char *data, int length, IPX_Address **addresses, int numPlayers,
    byte *randomVars, int *randomVarsLength)
{
    int i;
    const char *p = data + 2;
    assert(*(word *)data == PT_SYNC);
    if (length - 2 > numPlayers * sizeof(IPX_Address) + *randomVarsLength)
        return false;
    for (i = 0; i < numPlayers; i++) {
        memcpy(addresses[i], p, sizeof(IPX_Address));
        p += sizeof(IPX_Address);
    }
    memcpy(randomVars, p, *randomVarsLength = length - (p - data));
    return true;
}

char *createTeamAndTacticsPacket(int *length, const char *teamData, const Tactics *tacticsData)
{
    *(word *)packet = PT_TEAM_AND_TACTICS;
    memcpy(packet + 2, teamData, TEAM_SIZE);
    memcpy(packet + 2 + TEAM_SIZE, tacticsData, sizeof(Tactics) * 6);
    *length = 2 + TEAM_SIZE + sizeof(Tactics) * 6;
    assert(*length <= sizeof(packet));
    return packet;
}

bool unpackTeamAndTacticsPacket(const char *data, int length, char *teamData, Tactics *tacticsData)
{
    int i;
    assert(*(word *)data == PT_TEAM_AND_TACTICS);
    if (length != 2 + TEAM_SIZE + sizeof(Tactics) * 6)
        return false;
    memcpy(teamData, data + 2, TEAM_SIZE);
    memcpy(tacticsData, data + 2 + TEAM_SIZE, sizeof(Tactics) * 6);
    /* do some rudimentary checks on tactics - null terminate and range check ball out-of-play tactics */
    for (i = 0; i < 6; i++) {
        tacticsData[i].name[offsetof(Tactics, playerPos) - 1] = '\0';
        if (tacticsData[i].ballOutOfPlayTactics >= TACTICS_USER_A)
            tacticsData[i].ballOutOfPlayTactics = 0;
    }
    return true;
}

char *createControlsPacket(int *length, byte scanCode, word longFireFlag)
{
    *(word *)packet = PT_CONTROLS;
    packet[2] = scanCode;
    *(word *)(packet + 3) = longFireFlag;
    *length = sizeof(word) + sizeof(byte) + sizeof(word);
    assert(*length <= sizeof(packet));
    return packet;
}

bool unpackControlsPacket(const char *data, int length, byte *scanCode, word *longFireFlag)
{
    assert(*(word *)data == PT_CONTROLS);
    if (length != sizeof(word) + sizeof(byte) + sizeof(word))
        return false;
    *scanCode = data[2];
    *longFireFlag = *(word *)(data + 3);
    return true;
}