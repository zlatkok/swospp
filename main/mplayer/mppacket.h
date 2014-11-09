#pragma once

/* start them from higher number so they can be more distinctive in a packet */
enum MP_Packet_Type {
    PT_NONE = 0x2456,
    PT_GET_GAME_INFO,
    PT_JOIN_GAME,
    PT_GAME_INFO_DATA,
    PT_JOIN_GAME_REFUSED,
    PT_JOIN_GAME_ACCEPTED,
    PT_OPTIONS,
    PT_PLAYER_JOINED,
    PT_PLAYER_LEFT,
    PT_PLAYER_TEAM,
    PT_PLAYER_FLAGS,
    PT_CHAT_LINE,
    PT_PING,
    PT_SYNC,
    PT_SYNC_OK,
    PT_SYNC_FAIL,
    PT_SYNC_DONE,
    PT_TEAM_AND_TACTICS,
    PT_CONTROLS,
    PT_GAME_CONTROLS,
    PT_GAME_SPRITES
};

word getRequestType(const char *packet, int length);
char *createGetGameInfoPacket(int *length);
char *createGameInfoPacket(int *length, int numPlayers, char *gameName, byte id);
bool unpackGameInfoPacket(const char *data, int length, int *networkVersion, int *networkSubversion,
    int *numPlayers, int *maxPlayers, char *gameName, int maxGameNameLen, byte *id);
char *createJoinGamePacket(int *length, byte joinId, byte gameId, const char *name, const char *teamName);
bool unpackJoinGamePacket(const char *data, int length, word *networkVersion,
    word *networkSubversion, byte *joinId, byte *gameId, char *name, char *teamName);
char *createRefuseJoinGamePacket(int *length, byte errorCode);
bool unpackRefuseJoinGamePacket(const char *data, int length, byte *errorCode);
char *createJoinedGameOkPacket(int *length, const LobbyState *state, const IPX_Address **addresses);
bool unpackJoinedGameOkPacket(const char *data, int length, LobbyState *state, IPX_Address **addresses);
char *createOptionsPacket(int *length, const MP_Options *options);
bool unpackOptionsPacket(const char *data, int length, MP_Options *options);
char *createPlayerJoinedPacket(int *length, const char *playerName, const char *playerTeamName);
bool unpackPlayerJoinedPacket(const char *data, int length, char *playerName, char *playerTeamName);
char *createPlayerFlagsPacket(int *length, int flags, int playerIndex);
bool unpackPlayerFlagsPacket(const char *data, int length, int *playerIndex, int *flags);
char *createPlayerTeamChangePacket(int *length, int playerIndex, char *teamName);
bool unpackPlayerTeamChangePacket(const char *data, int length, int *playerIndex, char *teamName);
void setPlayerIndex(char *data, int playerIndex);
char *createPlayerLeftPacket(int *length, int playerIndex);
bool unpackPlayerLeftPacket(const char *data, int length, int *playerIndex);
char *createPlayerChatPacket(int *length, const char *text, byte color);
bool unpackPlayerChatPacket(const char *data, int length, char *text, byte *color);
char *createSyncPacket(int *length, const IPX_Address **addresses, int numPlayers,
    const byte *randomVars, int randomVarsLength);
bool unpackSyncPacket(const char *data, int length, IPX_Address **addresses, int numPlayers,
    byte *randomVars, int *randomVarsLength);
char *createTeamAndTacticsPacket(int *length, const char *teamData, const Tactics *tacticsData);
bool unpackTeamAndTacticsPacket(const char *data, int length, char *teamData, Tactics *tacticsData);
char *createControlsPacket(int *length, byte controls, word longFireFlag);
bool unpackControlsPacket(const char *data, int length, byte *controls, word *longFireFlag);