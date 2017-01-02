#pragma once

void SetDirectGameName(bool isServer, const char *start, const char *end);
void SetTimeout(const char *start, const char *end);
void SetCommFile(const char *start, const char *end);
void SetNickname(const char *start, const char *end);
void DirectModeOnCommandLineParsingDone();

extern "C" {
    const char *GetDirectGameName();
    const char *GetDirectGameNickname();
    const char *MainMenuSelect();
    void SearchForTheGameInit();
    void ShowErrorAndQuit(const char *message = nullptr);
    void UpdateGameSearch();
}
