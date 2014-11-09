#pragma once

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;
typedef unsigned int uint;
typedef unsigned char uchar;
typedef signed char schar;
typedef unsigned int bool;
typedef unsigned short ushort;

typedef dword uint32_t;
typedef signed int int32_t;
typedef word uint16_t;
typedef signed short int16_t;
typedef byte uint8_t;
typedef signed char int8_t;

typedef unsigned int size_t;

#define true  (1 == 1)
#define false (1 == 0)

#ifndef nullptr
#define nullptr ((void*)0)
#endif

#ifndef NULL
#define NULL nullptr
#endif