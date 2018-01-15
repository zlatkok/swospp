#include <cstdint>
#include <stdarg.h>

#pragma GCC diagnostic ignored "-Wparentheses"

/* Shamelessly stolen from Watcom :P */

char *ultoa(unsigned long value, char *buffer, unsigned radix);
static char *itoa (int value, char *buffer, int radix);
char *ltoa (long value, char *buffer, int radix);

#define BUF_SIZE 40

struct Specification {
    int width;
    int precision;
    int size_modifier;
    int type;
    int alignment;
    int force_decimal_point;
    int output_count;
    int length;
    int n0;
    int nz0;
    int n1;
    int nz1;
    int n2;
    int nz2;
    int zero_pad:1;
    int space_pad:1;
    int blank_prefix:1;
    int sign_prefix:1;
    int sharp:1;
    int is_short:1;
    int is_long:1;
    int is_short_short:1;
};

static void SetZeroPad(Specification *spec);
static void ResetSpecification(Specification *);
static const char *GetSpecification(const char *, Specification *, va_list *);
static char *FormString(Specification *, va_list *, char *);
static void FixedPointFormat(char *buf, int value, Specification *specs);
static void FloatFormat(char *buf, va_list *args, Specification *spec);

int __cdecl vsprintf(char *buf, const char *fmt, va_list args);

int __cdecl sprintf(char *buf, const char *fmt, ...)
{
    va_list va;
    int ret;

    va_start(va, fmt);
    ret = vsprintf(buf, fmt, va);
    va_end(va);

    return ret;
}

int __cdecl vsprintf(char *buf, const char *fmt, va_list args)
{
    const char *p;
    char *out = buf, *arg, *a;
    Specification spec;
    static char buffer[BUF_SIZE];

    /* initialize specification structure */
    memset(&spec, 0, sizeof(Specification));

    for (p = fmt; *p; p++) {
        if (*p != '%') {
            *out++ = *p;
            spec.output_count++;
        } else if (p[1] == '%') {
            *out++ = *p++;
            spec.output_count++;
        } else {
            ResetSpecification(&spec);
            p = GetSpecification(++p, &spec, &args);
            if (*p == 'n') {
                if (spec.is_long)
                    *va_arg(args, long int *) = spec.output_count;
                else if (spec.is_short)
                    *va_arg(args, short int *) = spec.output_count;
                else if (spec.is_short_short)
                    *va_arg(args, char *) = spec.output_count;
            } else {
                spec.type = *p;
                arg = FormString(&spec, &args, buffer);
                spec.width = spec.n0 + spec.nz0 + spec.n1 + spec.nz1 + spec.n2 + spec.nz2;
                if (spec.alignment >= 0 && spec.space_pad) {
                    memcpy(out, buffer, spec.width);
                    out += spec.width;
                    spec.output_count += spec.width;
                }
                a = buffer;
                memcpy(out, a, spec.n0);
                out += spec.n0;
                spec.output_count += spec.n0;
                a += spec.n0;
                memset(out, '0', spec.nz0);
                out += spec.nz0;
                spec.output_count += spec.nz0;
                if (spec.type == 's' && spec.sharp) {
                    unsigned char *p = (uchar *)arg, *q = (uchar *)out;
                    int n = (spec.n1 + 1) / 3;
                    while (n--) {
                        *q++ = hexDigits[*p >> 4];
                        *q++ = hexDigits[*p++ & 0xf];
                        if (n > 0)
                            *q++ = ' ';
                    }
                } else
                    memcpy(out, arg, spec.n1);

                out += spec.n1;
                arg += spec.n1;
                spec.output_count += spec.n1;
                memset(out, '0', spec.nz1);
                out += spec.nz1;
                spec.output_count += spec.nz1;
                memcpy(out, arg, spec.n2);
                out += spec.n2;
                arg += spec.n2;
                spec.output_count += spec.n2;
                memset(out, '0', spec.nz2);
                out += spec.nz2;
                spec.output_count += spec.nz2;

                if (spec.alignment < 0) {
                    memset(out, ' ', spec.width);
                    out += spec.width;
                    spec.output_count += spec.width;
                }
            }
        }
    }
    buf[spec.output_count] = '\0';
    return spec.output_count;
}

static const char *GetSpecification(const char *p, Specification *spec, va_list *arg)
{
    /* default is right align */
    spec->alignment = 1;
    spec->width = 0;

    /* get flags */
    for (;;) {
        if (*p =='-') {
            spec->alignment = -1;
        } else if (*p == '+') {
            spec->sign_prefix = true;
        } else if (*p == '0') {
            if (spec->alignment >= 0)
                spec->zero_pad = true;
        } else if (*p == ' ') {
            if (!spec->sign_prefix)
                spec->space_pad = true;
        } else if (*p == '#') {
            spec->sharp = true;
        } else {
            break;
        }
        p++;
    }

    /* get width */
    if (*p == '*') {
        spec->width = va_arg(*arg, int);
        if (spec->width < 0) {
            spec->width = -spec->width;
            spec->alignment = -1;
        }
    } else {
        while (*p >= '0' && *p <= '9')
            spec->width = 10 * spec->width + *p++ - '0';
    }

    /* get precision */
    spec->precision = -1;
    if (*p == '.') {
        spec->precision = 0;
        p++;
        if (*p == '*') {
            spec->precision = va_arg(*arg, int);
            if (spec->precision < 0)
                spec->precision = 0;
            p++;
        } else
            while (*p >= '0' && *p <= '9')
                spec->precision = 10 * spec->precision + *p++ - '0';
    }

    /* get prefix */
    switch (*p) {
    case 'l':
    case 'w':
        spec->is_long = true;
        p++;
        break;

    case 'h':
        if (p[1] == 'h') {
            spec->is_short_short = true;
            p += 2;
        } else {
            spec->is_short = true;
            p++;
        }
        break;
    }

    return p;
}

void ResetSpecification(Specification *spec)
{
    spec->alignment = 0;
    spec->sign_prefix = false;
    spec->zero_pad = spec->space_pad = spec->sign_prefix = spec->sharp = false;
    spec->is_short = spec->is_long = spec->is_short_short = false;
}

static char *FormString(Specification *spec, va_list *args, char *buffer)
{
    unsigned long long_value = 0;
    unsigned int int_value;
    int radix, i, len;
    char *arg = buffer, *tmp;

    spec->n0  = 0;
    spec->nz0 = 0;
    spec->n1  = 0;
    spec->nz1 = 0;
    spec->n2  = 0;
    spec->nz2 = 0;

    switch (spec->type) {
    case 'C':
        WriteToLog("FormString: wide characters not supported.");
        /* fall-through */

    case 'c':
        spec->length = 1;
        buffer[0] = va_arg(*args, int);
        break;

    case 'd':
    case 'i':
        long_value = spec->is_long ? va_arg(*args, long) : va_arg(*args, int);
        if (spec->is_short)
            long_value = (signed short)long_value;
        else if (spec->is_short_short)
            long_value = (signed char)long_value;
        if ((long)long_value < 0) {
            buffer[spec->n0++] = '-';
            long_value = -long_value;
        } else if (spec->sign_prefix) {
            buffer[spec->n0++] = '+';
        } else if (spec->blank_prefix) {
            buffer[spec->n0++] = ' ';
        }
        break;

    case 'o':
    case 'u':
    case 'x':
    case 'X':
        if (spec->is_long)
            long_value = va_arg(*args, unsigned long);
        else {
            long_value = va_arg(*args, unsigned int);
            if (spec->is_short)
                long_value = (unsigned short)long_value;
            else if (spec->is_short_short)
                long_value = (unsigned char)long_value;
        }
        break;
    }

    radix = 10;
    switch (spec->type) {
    case 'f':
    case 'F':
        int_value = va_arg(*args, long);
        FixedPointFormat(buffer, int_value, spec);
        spec->length = strlen(buffer);
        break;

    case 'g':
    case 'G':
    case 'e':
    case 'E':
        FloatFormat(buffer, args, spec);
        break;

    case 's':
        buffer[0] = '\0';
        tmp = va_arg(*args, char *);
        if (tmp)
            arg = tmp;

        spec->n1 = spec->length = strlen(arg);
        if (spec->precision >= 0 && spec->precision < spec->length)
            spec->n1 = spec->precision;
        if (spec->sharp) {
            if (spec->precision > spec->n1)
                spec->n1 = spec->precision;
            spec->n1 = spec->n1 * 3 - 1;
        }
        break;

    case 'x':
    case 'X':
        if (spec->sharp && long_value) {
            buffer[spec->n0++] = '0';
            buffer[spec->n0++] = spec->type;
        }
        radix = 16;
        /* fall-through */

    case 'o':
        if (spec->type == 'o') {
            radix = 8;
            if (spec->sharp)
                buffer[spec->n0++] = '0';
        }
        /* fall-through */

    case 'd':
    case 'i':
    case 'u':
        if (spec->precision != -1) {
            spec->space_pad = true;
            spec->zero_pad = false;
        }

        arg = &buffer[spec->n0];
        if (!spec->precision && !long_value) {
            *arg = '\0';
            spec->length = 0;
        } else {
            ultoa(long_value, &buffer[spec->n0], radix);
            if (spec->type == 'X')
                strupr(buffer);
            spec->length = strlen(arg);
        }

        spec->n1 = spec->length;
        if (spec->n1 < spec->precision)
            spec->nz0 = spec->precision - spec->n1;
        if (spec->precision == -1)
            SetZeroPad(spec);
        break;

    case 'p':
    case 'P':
        if (!spec->width)
            spec->width = 2 * sizeof(void *);

        spec->space_pad = spec->sign_prefix = false;
        int_value = va_arg(*args, unsigned);
        itoa(int_value, buffer, 16);
        len = strlen(buffer);

        for (i = 2 * sizeof(void *) - 1; len; i--)
            buffer[i] = buffer[--len];

        while (i >= 0)
            buffer[i--] = '0';

        buffer[2 * sizeof(void *)] = '\0';
        if (spec->type == 'X')
            strupr(buffer);

        spec->n0 = strlen(arg);
        break;

    case 'C':
    case 'c':
        spec->n1 = 1;
        break;

    default:
        spec->width = 0;
        buffer[0] = spec->type;
        spec->n0 = 1;
        break;
    }
    return arg;
}

static void SetZeroPad(Specification *spec)
{
    int n;

    if (spec->alignment >= 0) {
        if (spec->zero_pad) {
            n = spec->width - spec->n0 - spec->nz0 - spec->n1 - spec->nz1 -
                spec->n2 - spec->nz2;
            if (n > 0) {
                spec->nz0 += n;
            }
        }
    }
}

static void FixedPointFormat(char *buffer, int value, Specification *spec)
{
    FixedPoint f{ value };
    auto p = int2str(f.whole());
    p = strcpy(buffer, p);
    *p++ = '.';
    *p = '\0';

    // divide fraction with 2^16 to get the real value
    const int kMaxDigits = 16;
    const int64_t kQuant = 152587890625;

    int fraction = (f.fraction() * kQuant) >> 32;
    auto q = int2str(fraction < 0 ? -fraction : fraction);
    auto len = strlen(q);

    while (len++ < kMaxDigits)
        *p++ = '0';
    strcpy(p, q);

    spec->n1 = strlen(buffer);
}

static void FloatFormat(char *, va_list *, Specification *) {}

/* helper convert routines */
static const char kAlphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";

/* Get quotient and reminder using single division. */
static int __udiv(int value, int *divQuot)
{
    asm (
        "xor  edx, edx                  \n"
        "div dword ptr [%[divQuot]]     \n"
        "mov [%[divQuot]], edx          \n"
        : "+a" (value)
        : [divQuot] "r" (divQuot)
        : "cc", "edx", "memory"
    );
    return value;
}

static char *utoa(unsigned int value, char *buffer, int radix)
{
    char *p = buffer;
    char *q;
    unsigned rem;
    unsigned quot;
    char buf[34];

    buf[0] = '\0';
    q = &buf[1];
    do {
        quot = radix;
        rem = __udiv(value, (int *)&quot);
        *q = kAlphabet[rem];
        ++q;
        value = quot;
    } while (value != 0);

    while (*p++ = *--q);

    return buffer;
}

static char *itoa (int value, char *buffer, int radix)
{
    char *p = buffer;

    if (radix == 10 && value < 0) {
        *p++ = '-';
        value = -value;
    }

    utoa(value, p, radix);
    return buffer;
}

char *ultoa(unsigned long value, char *buffer, unsigned radix)
{
    char *p = buffer;
    char *q;
    unsigned rem;
    char buf[34];

    buf[0] = '\0';
    q = &buf[1];

    do {
        rem = radix;
        value = __udiv(value, (int *)&rem);
        *q = kAlphabet[rem];
        ++q;
    } while (value != 0);

    while (*p++ = *--q);
    return buffer;
}

char *ltoa (long value, char *buffer, int radix)
{
    char *p = buffer;

    if (radix == 10 && value < 0) {
        *p++ = '-';
        value = -value;
    }

    ultoa(value, p, radix);
    return buffer;
}
