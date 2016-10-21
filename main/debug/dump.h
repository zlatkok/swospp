#pragma once

enum flags_bits {
    ALIGN_LEFT_BIT,
    ALIGN_RIGHT_BIT,
    ALIGN_CENTERX_BIT,
    ALIGN_UP_BIT,
    ALIGN_DOWN_BIT,
    ALIGN_CENTERY_BIT,
    HEX_BIT,
    OCTAL_BIT,
    BIN_BIT,
    UNSIGNED_BIT,
    SIGN_PREFIX_BIT,
    BIG_FONT_BIT
};

#define NO_FLAGS             0
#define NO_ALIGNMENT         NO_FLAGS
#define ALIGN_LEFT           1 << ALIGN_LEFT_BIT
#define ALIGN_RIGHT          1 << ALIGN_RIGHT_BIT
#define ALIGN_CENTERX        1 << ALIGN_CENTERX_BIT
#define ALIGN_UP             1 << ALIGN_UP_BIT
#define ALIGN_DOWN           1 << ALIGN_DOWN_BIT
#define ALIGN_CENTERY        1 << ALIGN_CENTERY_BIT
#define FL_HEX               1 << HEX_BIT
#define FL_OCTAL             1 << OCTAL_BIT
#define FL_BIN               1 << BIN_BIT
#define FL_UNSIGNED          1 << UNSIGNED_BIT
#define FL_SIGN_PREFIX       1 << SIGN_PREFIX_BIT
#define FL_BIG_FONT          1 << BIG_FONT_BIT

#define ALIGN_CENTER      (ALIGN_CENTERX | ALIGN_CENTERY)
#define ALIGN_UPRIGHT     (ALIGN_RIGHT | ALIGN_UP)
#define ALIGN_UPLEFT      (ALIGN_LEFT | ALIGN_UP)
#define ALIGN_DOWNRIGHT   (ALIGN_DOWN | ALIGN_RIGHT)
#define ALIGN_DOWNLEFT    (ALIGN_DOWN | ALIGN_LEFT)

/* functions in printstr.c */
void GetStringLength(char *str, int *w, int *h, uint align, uint big);
void PrintString(char *str, int x, int y, bool big, int color, uint align);