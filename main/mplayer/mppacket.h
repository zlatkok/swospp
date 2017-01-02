#pragma once

#include "mplobby.h"

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

word GetRequestType(const char *packet, int length);
char *CreateGetGameInfoPacket(int *length);
char *CreateGameInfoPacket(int *length, int numPlayers, const char *gameName, byte id);
bool UnpackGameInfoPacket(const char *data, int length, int *networkVersion, int *networkSubversion,
    int *numPlayers, int *maxPlayers, char *gameName, int maxGameNameLen, byte *id);
char *CreateJoinGamePacket(int *length, byte joinId, byte gameId, const char *name, const char *teamName);
bool UnpackJoinGamePacket(const char *data, int length, word *networkVersion,
    word *networkSubversion, byte *joinId, byte *gameId, char *name, char *teamName);
char *CreateRefuseJoinGamePacket(int *length, byte errorCode);
bool UnpackRefuseJoinGamePacket(const char *data, int length, byte *errorCode);
char *CreateJoinedGameOkPacket(int *length, const LobbyState *state, const IPX_Address **addresses);
bool UnpackJoinedGameOkPacket(const char *data, int length, LobbyState *state, IPX_Address **addresses);
char *CreateOptionsPacket(int *length, const MP_Options *options);
bool UnpackOptionsPacket(const char *data, int length, MP_Options *options);
char *CreatePlayerJoinedPacket(int *length, const char *playerName, const char *playerTeamName);
bool UnpackPlayerJoinedPacket(const char *data, int length, char *playerName, char *playerTeamName);
char *CreatePlayerFlagsPacket(int *length, int flags, int playerIndex);
bool UnpackPlayerFlagsPacket(const char *data, int length, int *playerIndex, int *flags);
char *CreatePlayerTeamChangePacket(int *length, int playerIndex, char *teamName);
bool UnpackPlayerTeamChangePacket(const char *data, int length, int *playerIndex, char *teamName);
void SetPlayerIndex(char *data, int playerIndex);
char *CreatePlayerLeftPacket(int *length, int playerIndex);
bool UnpackPlayerLeftPacket(const char *data, int length, int *playerIndex);
char *CreatePlayerChatPacket(int *length, const char *text, byte color);
bool UnpackPlayerChatPacket(const char *data, int length, char *text, byte *color);
char *CreateSyncPacket(int *length, const IPX_Address **addresses, int numPlayers,
    const byte *randomVars, int randomVarsLength);
bool UnpackSyncPacket(const char *data, int length, IPX_Address **addresses, int numPlayers,
    byte *randomVars, int *randomVarsLength);
char *CreateTeamAndTacticsPacket(int *length, const TeamFile *teamData, const Tactics *tacticsData);
bool UnpackTeamAndTacticsPacket(const char *data, int length, TeamFile *teamData, Tactics *tacticsData);
char *CreateControlsPacket(int *length, byte controls, word longFireFlag);
bool UnpackControlsPacket(const char *data, int length, byte *controls, word *longFireFlag);
