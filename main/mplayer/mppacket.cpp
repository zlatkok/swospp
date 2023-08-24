/***

   SWOS++ Multiplayer packet handling routines
   ===========================================

***/

#include "dosipx.h"
#include "mplayer.h"
#include "mppacket.h"

const int kPacketSize = 3 * 1024;

static union {
    char data[kPacketSize];
    word wordData[kPacketSize / sizeof(word)];

    operator void *() {
        return data;
    }
} m_packet;

static_assert(kPacketSize % sizeof(word) == 0, "Packet size must contain whole number of words.");

word GetRequestType(const char *packet, int length)
{
    if (length < (int)sizeof(word))
        return PT_NONE;

    return *(word *)packet;
}

char *CreateGetGameInfoPacket(int *length)
{
    *m_packet.wordData = PT_GET_GAME_INFO;
    *length = sizeof(word);
    return m_packet.data;
}

char *CreateGameInfoPacket(int *length, int numPlayers, const char *gameName, byte id)
{
    char *p;
    word *w = m_packet.wordData;
    w[0] = PT_GAME_INFO_DATA;
    w[1] = NETWORK_VERSION;
    w[2] = NETWORK_SUBVERSION;
    w[3] = (numPlayers & 255) | (MAX_PLAYERS << 8);
    *(p = strcpy(m_packet.data + 8, gameName)) = id;
    *length = p - m_packet.data + 1;
    assert(*length <= (int)sizeof(m_packet));
    return m_packet.data;
}

bool UnpackGameInfoPacket(const char *data, int length, int *networkVersion, int *networkSubversion,
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

    auto nameLen = min(maxGameNameLen - 1, length - 8 - 1);
    ((char *)memcpy(gameName, data + 8, nameLen))[nameLen] = '\0';
    *id = data[length - 1];
    return true;
}

char *CreateJoinGamePacket(int *length, byte joinId, byte gameId, const char *name, const char *teamName)
{
    word *w = m_packet.wordData;
    w[0] = PT_JOIN_GAME;
    w[1] = NETWORK_VERSION;
    w[2] = NETWORK_SUBVERSION;
    m_packet.data[6] = joinId;
    m_packet.data[7] = gameId;
    *length = strcpy(strcpy(m_packet.data + 8, name) + 1, teamName) - m_packet.data;
    assert(*length <= (int)sizeof(m_packet));
    return m_packet.data;
}

bool UnpackJoinGamePacket(const char *data, int length, word *networkVersion,
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

char *CreateRefuseJoinGamePacket(int *length, byte errorCode)
{
    *m_packet.wordData = PT_JOIN_GAME_REFUSED;
    m_packet.data[2] = errorCode;
    assert(errorCode != GR_OK);
    *length = 3;
    return m_packet.data;
}

bool UnpackRefuseJoinGamePacket(const char *data, int length, byte *errorCode)
{
    if (length < 3)
        return false;

    *errorCode = data[2];
    return true;
}

char *CreateJoinedGameOkPacket(int *length, const LobbyState *state, const IPX_Address **addresses)
{
    char *p = m_packet.data;
    *(word *)p = PT_JOIN_GAME_ACCEPTED;
    p[2] = state->numPlayers;
    p += 3;

    for (int i = 0; i < state->numPlayers; i++) {
        *(p = strncpy(strcpy(p, state->playerNames[i]) + 1, state->playerTeamsNames[i], MAX_TEAM_NAME_LEN)) = '\0';
        *++p = state->playerFlags[i];
        memcpy(p + 1, addresses[i], sizeof(IPX_Address));
        p += sizeof(IPX_Address) + 1;
    }

    memcpy(p, &state->options, sizeof(MP_Options));
    *length = p - m_packet.data + sizeof(MP_Options);
    assert(*length <= (int)sizeof(m_packet));
    return m_packet.data;
}

bool UnpackJoinedGameOkPacket(const char *data, int length, LobbyState *state, IPX_Address **addresses)
{
    if (length < (int)(3 + 3 + sizeof(IPX_Address) + sizeof(MP_Options)))
        return false;

    assert(*(word *)data == PT_JOIN_GAME_ACCEPTED);

    if ((state->numPlayers = data[2]) > MAX_PLAYERS)
        return false;

    const char *p = data + 3;
    const char *limit = nullptr;

    for (int i = 0; i < state->numPlayers; i++) {
        /* player nickname */
        char *q = state->playerNames[i];
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
            CopyAddress(addresses[i], (IPX_Address *)p);
            p += sizeof(IPX_Address);
        } else
            return false;
    }

    if (p + sizeof(MP_Options) > limit)
        return false;

    /* and in the end options */
    memcpy(&state->options, p, sizeof(MP_Options));
    return true;
}

char *CreateOptionsPacket(int *length, const MP_Options *options)
{
    assert(sizeof(m_packet) >= sizeof(MP_Options) + sizeof(word));
    *m_packet.wordData = PT_OPTIONS;
    memcpy(m_packet.data + 2, options, sizeof(MP_Options));
    *length = sizeof(MP_Options) + sizeof(word);
    return m_packet.data;
}

bool UnpackOptionsPacket(const char *data, int length, MP_Options *options)
{
    assert(*(word *)data == PT_OPTIONS);

    if (length != sizeof(MP_Options) + sizeof(word))
        return false;

    memcpy(options, data + 2, sizeof(MP_Options));
    return true;
}

char *CreatePlayerJoinedPacket(int *length, const char *playerName, const char *playerTeamName)
{
    *m_packet.wordData = PT_PLAYER_JOINED;
    *length = strcpy(strcpy(m_packet.data + 2, playerName) + 1, playerTeamName) - m_packet.data;
    assert(*length + 1 <= (int)sizeof(m_packet));
    return m_packet.data;
}

bool UnpackPlayerJoinedPacket(const char *data, int length, char *playerName, char *playerTeamName)
{
    assert(*(word *)data == PT_PLAYER_JOINED);
    const char *p = data + 2, *limit = data + 2 + min(NICKNAME_LEN + 1, length - 2);

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

char *CreatePlayerFlagsPacket(int *length, int flags, int playerIndex)
{
    *m_packet.wordData = PT_PLAYER_FLAGS;
    m_packet.data[2] = playerIndex;
    m_packet.data[3] = flags;
    *length = 4;
    assert(*length <= (int)sizeof(m_packet));
    return m_packet.data;
}

bool UnpackPlayerFlagsPacket(const char *data, int length, int *playerIndex, int *flags)
{
    assert(*(word *)data == PT_PLAYER_FLAGS);

    if (length != 4)
        return false;

    *playerIndex = data[2];
    *flags = data[3];
    return true;
}

char *CreatePlayerTeamChangePacket(int *length, int playerIndex, char *teamName)
{
    *m_packet.wordData = PT_PLAYER_TEAM;
    m_packet.data[2] = playerIndex;
    *length = strcpy(m_packet.data + 3, teamName) - m_packet.data;
    assert(*length <= (int)sizeof(m_packet));
    return m_packet.data;
}

bool UnpackPlayerTeamChangePacket(const char *data, int length, int *playerIndex, char *teamName)
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

void SetPlayerIndex(char *data, int playerIndex)
{
    assert(*(word *)data == PT_PLAYER_TEAM || *(word *)data == PT_PLAYER_FLAGS ||
        *(word *)data == PT_PLAYER_LEFT);
    data[2] = playerIndex;
}

char *CreatePlayerLeftPacket(int *length, int playerIndex)
{
    *m_packet.wordData = PT_PLAYER_LEFT;
    m_packet.data[2] = playerIndex;
    *length = 3;
    return m_packet.data;
}

bool UnpackPlayerLeftPacket(const char *data, int length, int *playerIndex)
{
    assert(*(word *)data == PT_PLAYER_LEFT);

    if (length != 3)
        return false;

    *playerIndex = data[2];
    return true;
}

char *CreatePlayerChatPacket(int *length, const char *text, byte color)
{
    *m_packet.wordData = PT_CHAT_LINE;
    m_packet.data[2] = color;
    *length = strncpy(m_packet.data + 3, text, MAX_CHAT_LINE_LENGTH) - m_packet.data;
    assert(*length <= (int)sizeof(m_packet));
    return m_packet.data;
}

bool UnpackPlayerChatPacket(const char *data, int length, char *text, byte *color)
{
    assert(*(word *)data == PT_CHAT_LINE);

    if (length > (int)(MAX_CHAT_LINE_LENGTH + sizeof(word) + 1) || length < (int)sizeof(word) + 1)
        return false;

    *color = data[2];
    *strncpy(text, data + 3, min(MAX_CHAT_LINE_LENGTH, length - sizeof(word) - 1)) = '\0';

    return true;
}

char *CreateSyncPacket(int *length, const IPX_Address **addresses, int numPlayers,
    const byte *randomVars, int randomVarsLength)
{
    *m_packet.wordData = PT_SYNC;
    char *p = m_packet.data + 2;

    for (int i = 0; i < numPlayers; i++) {
        memcpy(p, addresses[i], sizeof(IPX_Address));
        p += sizeof(IPX_Address);
    }

    memcpy(p, randomVars, randomVarsLength);
    *length = p + randomVarsLength - m_packet.data;
    assert(*length <= (int)sizeof(m_packet));

    return m_packet.data;
}

bool UnpackSyncPacket(const char *data, int length, IPX_Address **addresses, int numPlayers,
    byte *randomVars, int *randomVarsLength)
{
    assert(*(word *)data == PT_SYNC);
    const char *p = data + 2;

    if (length - 2 > numPlayers * (int)sizeof(IPX_Address) + *randomVarsLength)
        return false;

    for (int i = 0; i < numPlayers; i++) {
        memcpy(addresses[i], p, sizeof(IPX_Address));
        p += sizeof(IPX_Address);
    }

    memcpy(randomVars, p, *randomVarsLength = length - (p - data));
    return true;
}

char *CreateTeamAndTacticsPacket(int *length, const TeamFile *team, const Tactics *tactics)
{
    *m_packet.wordData = PT_TEAM_AND_TACTICS;
    memcpy(m_packet.data + 2, team, sizeof(TeamFile));
    memcpy(m_packet.data + 2 + sizeof(TeamFile), tactics, sizeof(Tactics) * 6);
    *length = 2 + sizeof(TeamFile) + sizeof(Tactics) * 6;
    assert(*length <= (int)sizeof(m_packet));
    return m_packet.data;
}

bool UnpackTeamAndTacticsPacket(const char *data, int length, TeamFile *team, Tactics *tactics)
{
    assert(*(word *)data == PT_TEAM_AND_TACTICS);
    assert(team);
    assert(tactics);

    if (length != 2 + sizeof(TeamFile) + sizeof(Tactics) * 6)
        return false;

    memcpy(team, data + 2, sizeof(TeamFile));
    memcpy(tactics, data + 2 + sizeof(TeamFile), sizeof(Tactics) * 6);

    /* do some rudimentary checks on tactics - null terminate and range check ball out-of-play tactics */
    for (int i = 0; i < 6; i++) {
        tactics[i].name[offsetof(Tactics, playerPos) - 1] = '\0';
        if (tactics[i].ballOutOfPlayTactics >= TACTICS_USER_A)
            tactics[i].ballOutOfPlayTactics = 0;
    }

    return true;
}

char *CreateControlsPacket(int *length, byte scanCode, word longFireFlag)
{
    *m_packet.wordData = PT_CONTROLS;
    m_packet.data[2] = scanCode;
    m_packet.data[3] = longFireFlag & 0xff;
    m_packet.data[4] = longFireFlag >> 8;
    *length = sizeof(word) + sizeof(byte) + sizeof(word);
    assert(*length <= (int)sizeof(m_packet));
    return m_packet.data;
}

bool UnpackControlsPacket(const char *data, int length, byte *scanCode, word *longFireFlag)
{
    assert(*(word *)data == PT_CONTROLS);
    if (length != sizeof(word) + sizeof(byte) + sizeof(word))
        return false;
    *scanCode = data[2];
    *longFireFlag = *(word *)(data + 3);
    return true;
}
