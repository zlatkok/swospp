#pragma once

typedef void __cdecl (RegisterOptionsFunc)(const char *, int, const char *, int, const char *format, ...);

extern "C" void InitializeOptions();
extern "C" void SaveOptionsIfNeeded();