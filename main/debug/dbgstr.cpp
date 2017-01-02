#include "dump.h"

/** Char2Sprite

    c   - character to convert
    big - true or false for big character set

    returns:    255 - character has no sprite representation
                254 - new line
                253 - space
                252 - horizontal tab
                251 - vertical tab
                250 - anchor
             <= 249 - sprite index
*/
uchar Char2Sprite(char c, uint big)
{
    uchar spr;
    static uchar cvtTable[] = {
       /* 0    1    2    3    4    5    6    7    8    9  */
/* 0 */  255, 255, 255, 255, 255, 255, 255, 255, 255, 252,
/* 1 */  254, 251, 255, 250, 255, 255, 255, 255, 255, 255,
/* 2 */  255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
/* 3 */  255, 255, 253, 255, 255, 255, 255,  51, 255,   1,
/* 4 */    2,   3,  45,  50,   4,  47,   6,   7,   8,   9,
/* 5 */   10,  11,  12,  13,  14,  15,  16,  17,  49,  52,
/* 6 */  255, 255, 255,  48, 255,  18,  19,  20,  21,  22,
/* 7 */   23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
/* 8 */   33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
/* 9 */   43};

    c = toupper(c);
    if ((uchar)c > sizeof(cvtTable) - 1)
        spr = 255;
    else {
        spr = cvtTable[(unsigned)c];
        spr += big ? (spr > sizeof(cvtTable) - 1) ? 0 : 57 : 0;
    }
    return spr;
}

/** GetStringLength

    str   - string to "measure"
    w     - pointer to write width to
    h     - pointer to write height to
    align - true for alignment by column, or false for centering each line
    big   - boolean, is this a big font?

    Calculates number of pixels needed to draw string (width and height).
    Align is relevant only for multiline strings.

    Special characters:
    new line - After a new line, 1 pixel is added for small font; 2 for big.
    space    - 6 pixels are skipped for big, and 4 for small font.
    hor. tab - Size is 64 pixels for big, 32 pixels for small font.
    ver. tab - Skip 2 lines.
    anchor   - Marks the spot where the next line will begin. It is represented
               with carriage return ('\r'). For example, string
               "one line - \ranchor here\nsecond line" would print as:
               one line - anchor here
                          second line
               Only last anchor is relevant - string
               "one \rline - \ranchor here\nsecond line" gives the same result.
               Anchors are ignored when strings are x centered.
*/
void GetStringLength(char *str, int *w, int *h, uint align, uint big)
{
    int dx, dy = 6 + 2 * big, len = 0, height = dy, anchor = 0;
    uchar spr;

    for (; *str; str++) {
        switch (*str) {
        case ' ':
            len += big ? 6 : 4;
            break;
        case '\n':
            GetStringLength(++str, &dx, &dy, align, big);
            height += dy + 2 * big;
            dx += anchor;
            if (!align)
                len = max(len, dx);
            *w = len; *h = height;
            return;
        case '\t':
            /* we will calculate maximum tab value here, it might be less than
               this when printed - well, tabs ARE evil */
            len += big ? 64 : 32;
            break;
        case '\v':
            /* skip two lines */
            height += big ? 20 : 14;
            break;
        case '\r': /* mark anchor */
            anchor = len;
            break;
        default:
            spr = Char2Sprite(*str, big);
            if (spr == 255)
                continue;
            dx = spritesIndex[spr]->width;
            dy = spritesIndex[spr]->nlines;
            len += dx + 2 * big + 1;
        }
    }
    *w = len; *h = height;
}

/** PrintString

    str   - pointer to string to write
    x     - string x coordinate (may be ignored)
    y     - string y coordinate       -||-
    big   - write with big font?
    color - color for letters, 0 = original color
    align - alignment flags, can be:
            NO_ALIGNMENT  - string is written to x and y coordinates
            ALIGN_LEFT    - horizontal starting point is x coordinate
            ALIGN_RIGHT   - start x pixels from right
            ALIGN_CENTERX - centers string horizontally
            ALIGN_UP      - vertical starting point is y coordinate
            ALIGN_DOWN    - start y pixels from down
            ALIGN_CENTERY - centers string vertically

            Combinations are:
            ALIGN_CENTER    = ALIGN_CENTERX | ALIGN_CENTERY
            ALIGN_UPRIGHT   = ALIGN_RIGHT | ALIGN_UP
            ALIGN_UPLEFT    = ALIGN_LEFT | ALIGN_UP
            ALIGN_DOWNRIGHT = ALIGN_DOWN | ALIGN_RIGHT
            ALIGN_DOWNLEFT  = ALIGN_DOWN | ALIGN_LEFT

    For special characters, see GetStringLength.
*/
void PrintString(char *str, int x, int y, bool big, int color, uint align)
{
    uchar c;
    int dx, dy, xorig, xalign, yalign, anchor = 0;

    if ((align >> ALIGN_LEFT_BIT & 1) + (align >> ALIGN_RIGHT_BIT & 1) +
        (align >> ALIGN_CENTERX_BIT & 1) > 1) {
        WriteToLog("PrintString: Invalid flags for x alignment: 0x%x", align);
        return;
    }

    if ((align >> ALIGN_UP_BIT & 1) + (align >> ALIGN_DOWN_BIT & 1) +
        (align >> ALIGN_CENTERY_BIT & 1) > 1) {
        WriteToLog("PrintString: Invalid flags for y alignment: 0x%x", align);
        return;
    }

    GetStringLength(str, &dx, &dy, align & ALIGN_CENTERX, big);
    dx = min(dx, WIDTH);
    dy = min(dy, HEIGHT);

    xalign = align & (ALIGN_RIGHT | ALIGN_LEFT | ALIGN_CENTERX);
    if (xalign == ALIGN_RIGHT)
        x = WIDTH - dx;
    else if (xalign == ALIGN_LEFT)
        x = 0;
    else if (xalign == ALIGN_CENTERX)
        x = (WIDTH - dx) / 2;

    yalign = align & (ALIGN_UP | ALIGN_DOWN | ALIGN_CENTERY);
    if (yalign == ALIGN_UP)
        y = 0;
    else if (yalign == ALIGN_DOWN)
        y = HEIGHT - dy;
    else if (yalign == ALIGN_CENTERY)
        y = (HEIGHT - dy) / 2;

    xorig = x;
    for (c = Char2Sprite(*str, big); *str; c = Char2Sprite(*++str, big)) {
        switch (c) {
        case  253: /* space */
            x += big ? 6 : 4;
            break;
        case  254: /* new line */
            y += dy + (big ? 2 : 1);
            if (xalign & ALIGN_CENTERX) {
                GetStringLength(str + 1, &dx, &dy, true, big);
                dx = min(dx, WIDTH);
                x = (WIDTH - dx) / 2;
            } else
                x = xorig + anchor;
            anchor = 0;
            break;
        case 255:  /* can't draw this */
            WriteToLog("Can't draw this character: 0x%02x", *str);
            continue;
        case 252:  /* horizontal tab */
            x += big ? 64 : 32;
            x &= big ? ~63 : ~31;
            break;
        case 251:  /* vertical tab */
            y += 2 * (big ? 8 : 6);
            break;
        case 250:  /* anchor */
            anchor = x - xorig;
            break;
        default:   /* draw character */
            dx = spritesIndex[c]->width;
            dy = spritesIndex[c]->nlines;
            D0 = (dword)vsPtr + y * GAME_WIDTH + x;
            D4 = spritesIndex[c]->wquads * 16;
            D5 = dy;
            calla(SaveSprite);
            D0 = c;
            D1 = x;
            D2 = y;
            D3 = color;
            calla(DrawSpriteInColor);
            x += dx + 1;
        }
    }
}

/** PrintNumber

   num   - number to print
   x     - number x coordinate
   y     - number y coordinate
   big   - write with big font?

   Prints number using sprites.
*/
void PrintNumber(int num, uint x, uint y, bool big)
{
    PrintString(int2str(num), x, y, big, 2, NO_ALIGNMENT);
}