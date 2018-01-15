#include "dump.h"

#pragma GCC diagnostic ignored "-Wparentheses"

#define set(var, type, ptr) (var = *((type*)ptr))
#define set_inc(var, type, ptr) (set(var, type, ptr), ptr += sizeof(type))
#define get(type, ptr) (*(type*)ptr)

/* provided in dumpvars.asm */
extern char dump_vars[];

static void *data, *name;
static dword flags, dumpLen;
static word hexRows, hexCols;
static short x, y;
static int radix;
static byte disabled;

/* variables for controling output */
static int y_up_left, y_up_right, y_down_left, y_down_right;

void DebugPrintString(char *str, uint x, uint y, int color, dword flags)
{
    bool big = !!(flags & FL_BIG_FONT);
    int dx, dy = 6 + 2 * big;
    char buf[2048];

    buf[0] = '\0';
    if (name)
        strcpy(strcpy(buf, (const char *)name), ": ");
    strcat(buf, str);

    if ((flags & ALIGN_UPLEFT) == ALIGN_UPLEFT) {
        PrintString(buf, 0, y_up_left += dy, big, color, 0);
    } else if ((flags & ALIGN_UPRIGHT) == ALIGN_UPRIGHT) {
        GetStringLength(buf, &dx, &dy, 0, big);
        PrintString(buf, WIDTH - dx, y_up_right += dy, big, color, 0);
    } else if ((flags & ALIGN_DOWNLEFT) == ALIGN_DOWNLEFT) {
        PrintString(buf, 0, y_down_left -= dy, big, color, 0);
    } else if ((flags & ALIGN_DOWNRIGHT) == ALIGN_DOWNRIGHT) {
        GetStringLength(buf, &dx, &dy, 0, big);
        PrintString(buf, WIDTH-dx, y_down_right += dy, big, color, 0);
    } else
        PrintString(buf, x, y, big, color, 0);
}

/** Convert

    n   - number to convert
    buf - buffer to store string representation

    Returns pointer to terminating zero.

    Reads static var radix to see to which radix to convert,
*/
char *Convert(unsigned int n, char *inBuf)
{
    int c, i = 0, j;
    auto buf = (unsigned char *)inBuf;

    if ((int)n < 0 && !(flags & FL_UNSIGNED)) {
        *buf++ = '-';
        n = -(int)n;
    }

    do {
        c = n % radix;
        c += '0' + 7 * (c > 9);
        buf[i++] = tolower(c);
        n /= radix;
    } while (n);

    buf[i] = '\0';

    for (j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = tmp;
    }

    return (char *)buf + i;
}

void OutputChar(char c)
{
    char buf[9];

    Convert(c, buf);
    DebugPrintString(buf, x, y, 2, flags);
}

void OutputNumber(unsigned int c)
{
    char buf[35];
    int sign = false, i = 2;

    if ((int)c < 0 && !(flags & FL_UNSIGNED)) {
        c = -(int)c;
        sign = true;
    }

    Convert(c, buf + 2);

    if (flags & FL_HEX) {
        buf[i = 0] = '0';
        buf[1] = 'x';
    } else if (flags & FL_OCTAL) {
        buf[i = 1] = '0';
    } else if (!(flags & FL_BIN) && sign) {
        buf[i = 1] = '-';
    }

    DebugPrintString(buf + i, x, y, 2, flags);
}

void OutputString(char *str)
{
    char buf[16 + 1];
    int i = 0;

    while (i < 16 && (buf[i++] = *str++));
    buf[16] = '\0';

    DebugPrintString(buf, x, y, 2, flags);
}

/* ConvertFrac

   buf    - buffer to store string representation
   frac   - fracional part
   digits - how many digits to extract

   Returns pointer to terminating zero in buffer.

   Converts fractional part of fixed point number.
*/
char *ConvertFrac(char *buf, int frac, int digits)
{
    /* consider negative fractions */
    if (frac > 0x7fff)
        frac = 0x10000 - frac;
    frac *= 10;
    while (digits-- && frac) {
        int rem = frac / 32768;
        if (rem > 0)
            frac -= rem * 32768;
        frac *= 10;
        *buf++ = rem + '0';
    }

    while (digits-- > 0)
        *buf++ = '0';

    /* round up */
    buf[-1] += frac / 32768 > 5;
    *buf = '\0';

    return buf;
}

void OutputFixedPoint(int fixed)
{
    char buf[13], *p;

    p = Convert(fixed >> 16, buf);
    *p++ = '.';
    ConvertFrac(p, fixed & 0xffff, 5);

    DebugPrintString(buf, x, y, 2, flags);
}

void OutputMantissa(int fixed)
{
    char buf[8];

    buf[0] = '.';
    ConvertFrac(buf + 1, fixed & 0xffff, 5);

    DebugPrintString(buf, x, y, 2, flags);
}

void HexDump(char *start)
{
    char buf[16 * 17];
    int i, j;

    hexRows = hexRows > 16 ? 16 : hexRows;
    hexCols = hexCols > 16 ? 16 : hexCols;

    for (i = 0; i < hexRows; i++) {
        for (j = 0; j < hexCols; j++) {
            if (!dumpLen--) {
                i = hexRows;
                break;
            }
            buf[16 * i + j] = *start++;
        }
        buf[16 * i + j] = '\n';
    }
    DebugPrintString(buf, x, y, 2, flags);
}

extern "C" void DumpVariables()
{
    char code, *p = dump_vars;

    /* reset output variables */
    y_up_left = 18;
    y_up_right = 0;
    y_down_left = y_down_right = HEIGHT;

    /* code zero terminates variables array */
    while (code = *p++) {
        if (code == 'n')
            continue;

        if (code == 'i') {
            disabled = true;
            continue;
        }

        x = y = -1;
        set_inc(flags, word, p);

        if (code == 'u') {
            set_inc(dumpLen, dword, p);
            set_inc(hexRows, word, p);
            set_inc(hexCols, word, p);
            break;
        }

        set_inc(data, void *, p);

        name = nullptr;
        if (*p == '.')
            p++, set_inc(name, void *, p);

        if (disabled)
            continue;

        /* set radix */
        radix = 10;
        if (flags & FL_HEX)
            radix = 16;
        if (flags & FL_BIN)
            radix = 2;
        if (flags & FL_OCTAL)
            radix = 8;

        switch (code) {
        case 'c' : OutputChar(get(char, data));       break;
        case 'b' : OutputNumber(get(char, data));     break;
        case 'w' : OutputNumber(get(short, data));    break;
        case 'd' : OutputNumber(get(int, data));      break;
        case 's' : OutputString(get(char *, data));   break;
        case 'f' : OutputFixedPoint(get(int, data));  break;
        case 'm' : OutputMantissa(get(short, data));  break;
        case 'u' : HexDump(get(char *, data));        break;
        case 'p' : OutputNumber((uint)data);          break;
        default  : FatalError("Invalid code in dump variables stream!");
        }
        disabled = false;
    }
}

extern "C" void ToggleDebugOutput()
{
    disabled ^= 1;
}
