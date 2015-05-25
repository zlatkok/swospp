#ifndef ZK_H
#define ZK_H

/* turn on structure packing */
#if defined __WATCOMC__ || _MSC_VER || __GNUC__
#pragma pack(push, 1)
#else
#error "Please define structure packing directive for your compiler"
#endif

typedef struct {
    char  signature[4];
    word  verMajor;
    word  verMinor;
    dword headerSize;
    dword checksum;
    dword codeOffset;
    dword codeSize;
    dword patchOffset;
    dword patchSize;
    dword relocOffset;
    dword relocSize;
    dword bssSize;
    dword entryPoint;
} ZK_header;

#if defined __WATCOMC__ || _MSC_VER || __GNUC__
#pragma pack(pop)
#else
#error "Please define structure packing directive for your compiler"
#endif

#endif