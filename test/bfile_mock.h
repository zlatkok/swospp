#pragma once

/* set this up so we can use DOS file calls without needing to change the code :P */
extern void SetupWinFileEmulationLayer(unsigned char *start, const unsigned char *end);
extern int HandleException(unsigned long, int);