#ifndef ZK_H
#define ZK_H

/* turn on structure packing */
#if defined __WATCOMC__ || _MSC_VER
#pragma pack(push, 1)
#elif defined __GNUC__
#pragma align 1
#else
#error "Please define structure packing directive for your compiler"
#endif

typedef struct {
    char  signature[4];
    word  ver_major;
    word  ver_minor;
    dword header_size;
    dword checksum;
    dword code_ofs;
    dword code_size;
    dword patch_offset;
    dword patch_size;
    dword reloc_ofs;
    dword reloc_size;
    dword bss_size;
    dword entry_point;
} ZK_header;

#if defined __WATCOMC__ || _MSC_VER
#pragma pack(pop)
#elif defined __GNUC__
/* don't know */
#else
#error "Please define structure packing directive for your compiler"
#endif

#endif