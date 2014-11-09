#pragma once

void InitWatchers(bool weAreWatching);
void CleanupWatchers();
bool HandleWatcherPacket(const char *packet, int length, int currentTick, bool *abort);
bool WatcherTimeout(word networkTimeout);
char *CreateWatcherPacket(int *length, int currentFrameNo);
void ApplyWatcherPacket();
const char *GetEndGameWatcherPacket(int *length);