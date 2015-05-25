/***

   SWOS++ Multiplayer packet handling routines
   ===========================================

***/

#include "types.h"
#include "util.h"
#include "dosipx.h"
#include "mplayer.h"
#include "mppacket.h"

static const int PACKET_SIZE = 3 * 1024;

static union {
    char data[PACKET_SIZE];
    word wordData[PACKET_SIZE / sizeof(word)];
} packet;

word getRequestType(const char *packet, int length)
{
    if (length < (int)sizeof(word))
        return PT_NONE;
    return *(word *)packet;
}

char *createGetGameInfoPacket(int *length)
{
    *packet.wordData = PT_GET_GAME_INFO;
    *length = sizeof(word);
    return packet.data;
}

char *createGameInfoPacket(int *length, int numPlayers, char *gameName, byte id)
{
    char *p;
    word *w = packet.wordData;
    w[0] = PT_GAME_INFO_DATA;
    w[1] = NETWORK_VERSION;
    w[2] = NETWORK_SUBVERSION;
    w[3] = (numPlayers & 255) | (MAX_PLAYERS << 8);
    *(p = strcpy(packet.data + 8, gameName)) = id;
    *length = p - packet.data + 1;
    assert(*length <= (int)sizeof(packet));
    return packet.data;
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
    word *w = packet.wordData;
    w[0] = PT_JOIN_GAME;
    w[1] = NETWORK_VERSION;
    w[2] = NETWORK_SUBVERSION;
    packet.data[6] = joinId;
    packet.data[7] = gameId;
    *length = strcpy(strcpy(packet.data + 8, name) + 1, teamName) - packet.data;
    assert(*length <= (int)sizeof(packet));
    return packet.data;
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
    *packet.wordData = PT_JOIN_GAME_REFUSED;
    packet.data[2] = errorCode;
    assert(errorCode != GR_OK);
    *length = 3;
    return packet.data;
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
    char *p = packet.data;
    *(word *)p = PT_JOIN_GAME_ACCEPTED;
    p[2] = state->numPlayers;
    for (p += 3, i = 0; i < state->numPlayers; i++) {
        *(p = strncpy(strcpy(p, state->playerNames[i]) + 1, state->playerTeamsNames[i], MAX_TEAM_NAME_LEN)) = '\0';
        *++p = state->playerFlags[i];
        memcpy(p + 1, addresses[i], sizeof(IPX_Address));
        p += sizeof(IPX_Address) + 1;
    }
    memcpy(p, &state->options, sizeof(MP_Options));
    *length = p - packet.data + sizeof(MP_Options);
    assert(*length <= (int)sizeof(packet));
    return packet.data;
}

bool unpackJoinedGameOkPacket(const char *data, int length, LobbyState *state, IPX_Address **addresses)
{
    int i;
    const char *p, *limit = nullptr;
    char *q;
    if (length < (int)(3 + 3 + sizeof(IPX_Address) + sizeof(MP_Options)))
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
    *packet.wordData = PT_OPTIONS;
    memcpy(packet.data + 2, options, sizeof(MP_Options));
    *length = sizeof(MP_Options) + sizeof(word);
    return packet.data;
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
    *packet.wordData = PT_PLAYER_JOINED;
    *length = strcpy(strcpy(packet.data + 2, playerName) + 1, playerTeamName) - packet.data;
    assert(*length + 1 <= (int)sizeof(packet));
    return packet.data;
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
    *packet.wordData = PT_PLAYER_FLAGS;
    packet.data[2] = playerIndex;
    packet.data[3] = flags;
    *length = 4;
    assert(*length <= (int)sizeof(packet));
    return packet.data;
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
    *packet.wordData = PT_PLAYER_TEAM;
    packet.data[2] = playerIndex;
    *length = strcpy(packet.data + 3, teamName) - packet.data;
    assert(*length <= (int)sizeof(packet));
    return packet.data;
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
    *packet.wordData = PT_PLAYER_LEFT;
    packet.data[2] = playerIndex;
    *length = 3;
    return packet.data;
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
    *packet.wordData = PT_CHAT_LINE;
    packet.data[2] = color;
    *length = strncpy(packet.data + 3, text, MAX_CHAT_LINE_LENGTH) - packet.data;
    assert(*length <= (int)sizeof(packet));
    return packet.data;
}

bool unpackPlayerChatPacket(const char *data, int length, char *text, byte *color)
{
    assert(*(word *)data == PT_CHAT_LINE);
    if (length > (int)(MAX_CHAT_LINE_LENGTH + sizeof(word) + 1) || length < (int)sizeof(word) + 1)
        return false;
    *color = data[2];
    *strncpy(text, data + 3, min(MAX_CHAT_LINE_LENGTH, length - sizeof(word) - 1)) = '\0';
    return true;
}

char *createSyncPacket(int *length, const IPX_Address **addresses, int numPlayers,
    const byte *randomVars, int randomVarsLength)
{
    int i;
    char *p = packet.data + 2;
    *packet.wordData = PT_SYNC;
    for (i = 0; i < numPlayers; i++) {
        memcpy(p, addresses[i], sizeof(IPX_Address));
        p += sizeof(IPX_Address);
    }
    memcpy(p, randomVars, randomVarsLength);
    *length = p + randomVarsLength - packet.data;
    assert(*length <= (int)sizeof(packet));
    return packet.data;
}

bool unpackSyncPacket(const char *data, int length, IPX_Address **addresses, int numPlayers,
    byte *randomVars, int *randomVarsLength)
{
    int i;
    const char *p = data + 2;
    assert(*(word *)data == PT_SYNC);
    if (length - 2 > numPlayers * (int)sizeof(IPX_Address) + *randomVarsLength)
        return false;
    for (i = 0; i < numPlayers; i++) {
        memcpy(addresses[i], p, sizeof(IPX_Address));
        p += sizeof(IPX_Address);
    }
    memcpy(randomVars, p, *randomVarsLength = length - (p - data));
    return true;
}

char *createTeamAndTacticsPacket(int *length, const TeamFile *teamData, const Tactics *tacticsData)
{
    *packet.wordData = PT_TEAM_AND_TACTICS;
    memcpy(packet.data + 2, teamData, TEAM_SIZE);
    memcpy(packet.data + 2 + TEAM_SIZE, tacticsData, sizeof(Tactics) * 6);
    *length = 2 + TEAM_SIZE + sizeof(Tactics) * 6;
    assert(*length <= (int)sizeof(packet));
    return packet.data;
}

bool unpackTeamAndTacticsPacket(const char *data, int length, TeamFile *teamData, Tactics *tacticsData)
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
    *packet.wordData = PT_CONTROLS;
    packet.data[2] = scanCode;
    packet.data[3] = longFireFlag & 0xff;
    packet.data[4] = longFireFlag >> 8;
    *length = sizeof(word) + sizeof(byte) + sizeof(word);
    assert(*length <= (int)sizeof(packet));
    return packet.data;
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