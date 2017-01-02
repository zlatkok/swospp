#pragma once

typedef void __cdecl (RegisterOptionsFunc)(const char *, int, const char *, int, const char *format, ...);

const char *GetStringEnd(const char *start);

extern "C" {
    void InitializeOptions();
    void SaveOptionsIfNeeded();
    void SetDOSBoxDefaultOptions();
    void ParseCommandLine();
    bool GetUseBIOSJoystickRoutineOption();
    bool GetCalibrateJoysticksOption();
    bool DOSBoxDetected();
}
