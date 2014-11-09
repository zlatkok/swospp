#pragma once

typedef void __cdecl (RegisterOptionsFunc)(const char *, int, const char *, int, const char *format, ...);

void InitializeOptions();
void SaveOptionsIfNeeded();