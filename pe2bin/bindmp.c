/* bindmp - utility to dump ZK binary format, useful for validating binary
   file
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pe.h"
#include "zk.h"

static uint Checksum(dword c, int i, unsigned char *mem, int mem_size)
{
    int j;
    for (j = 0; j < mem_size; mem++, j++, i++) {
        c += i + *mem;
        c <<= 1;
        c ^= i + *mem;
    }
    return c;
}


static void saferead(char *fname, void *buf, size_t size, size_t cnt, FILE *f)
{
    if (fread(buf, size, cnt, f) != cnt && size) {
        fprintf(stderr, "Read error in file %s.\nsize = %d count = %d "
                "pos = %d", fname, size, cnt, ftell(f));
        exit(1);
    }
}

static void safeseek(char *fname, FILE *f, long ofs, int origin)
{
    if (fseek(f, ofs, origin)) {
        fprintf(stderr, "Seek error in file %s.\npos = %d origin = %d\n",
                fname, ofs, origin);
        exit(1);
    }
}

static void *xmalloc(size_t size)
{
    void *p = (void*)malloc(size);
    if (p)
        return p;
    fprintf(stderr, "xmalloc: Out of memory.\nRequest size: %d\n", size);
    exit(1);
}

#define xfree(a) free(a)

static void HexDump(uchar *p, uint size)
{
    uint i;
    int j, k;
    char *saved;
    for (i = 0; i < size; ) {
        printf("0x%08x:", i);
        for (saved = p, j = 0; j < 16 && i < size; j++, i++) {
            printf(" %02x", *p++);
        }
        putchar(' ');
        for (k = j; k < 16; k++) {
            putchar(' ');
            putchar(' ');
            putchar(' ');
        }
        for (p = saved; j > 0; j--) {
            putchar(*p < 32 || *p > 127 ? '.' : *p);
            p++;
        }
        putchar('\n');
    }
}

static dword *WriteRelocs(dword *p, int limit)
{
    while (*p != -1) {
        if (*p > limit)
            printf("<< INVALID RELOCATION >> 0x%08x > 0x%08x\n", *p++, limit);
        else
            printf("0x%08x\n", *p++);
    }
    return p + 1;
}

static void WritePatchData(uchar *p, int patch_size)
{
    int i, size, addr;
    for (i = 0; i < patch_size; ) {
        size = *p++;
        addr = *(dword*)p;
        p += 4;
        if (addr == -1)
            return;
        printf(size ? "[data block] " : "[fill block] ");
        printf("offset: 0x%08x ", addr);
        if (size) {
            printf("length: %d\n", size);
            HexDump(p, size);
            p += size;
            i += size + 5;
        } else {
            printf("repeat value %d, fill value 0x%02x\n", p[0], p[1]);
            p += 2;
            i += 7;
        }
    }
}


int main(int argc, char **argv)
{
    FILE *in;
    ZK_header *zk;
    int file_size, c, size;
    uchar *contents;
    dword *rel;

    fprintf(stderr, "bindmp - utility for dumping ZK binary files\n");
    fprintf(stderr, "Copyright Zlatko Karakas 2003, 2004.\n\n");

    if (argc < 2) {
        fprintf(stderr, "Input filename missing.\n");
        return 1;
    }

    if (!(in = fopen(argv[1], "rb"))) {
        fprintf(stderr, "Can't open %s.\n", argv[1]);
        return 1;
    }

    safeseek(argv[1], in, 0, SEEK_END);
    file_size = ftell(in);
    rewind(in);

    contents = (uchar*)xmalloc(file_size);
    saferead(argv[1], contents, file_size, 1, in);

    zk = (ZK_header*)contents;

    if (strncmp(zk->signature, "ZKBF", 4)) {
        fprintf(stderr, "Invalid ZK signature: %4s\n", zk->signature);
        return 1;
    }

    c = Checksum(0, size = 0, contents + zk->code_ofs, zk->code_size);
    c = Checksum(c, size += zk->code_size, contents + zk->patch_offset,
                 zk->patch_size);
    c = Checksum(c, size += zk->patch_size, contents + zk->reloc_ofs,
                 zk->reloc_size);

    /* print header */
    printf("[%s]\n\n", argv[1]);
    printf("File size:             %d\n", file_size);
    printf("Signature:             %4.4s\n", zk->signature);
    printf("Version:               %d.%d\n", zk->ver_major, zk->ver_minor);
    printf("Header size:           %d\n", zk->header_size);
    printf("Checksum:              %u (%u)\n", zk->checksum, c);
    printf("Offset to code:        %d\n", zk->code_ofs);
    printf("Size of code:          %d\n", zk->code_size);
    printf("Offset to patch data:  %d\n", zk->patch_offset);
    printf("Size of patch data:    %d\n", zk->patch_size);
    printf("Offset to relocations: %d\n", zk->reloc_ofs);
    printf("Size of relocations:   %d\n", zk->reloc_size);
    printf("BSS size:              %d\n", zk->bss_size);
    printf("Entry point:           %d\n", zk->entry_point);

    putchar('\n');
    putchar('\n');

    printf("Code and data section hex dump:\n\n");
    HexDump(contents + zk->code_ofs, zk->code_size);

    printf("\nPatch data:\n\n");
    WritePatchData(contents + zk->patch_offset, zk->patch_size);

    printf("\nRelocations code & data => SWOS code\n");
    rel = WriteRelocs((dword*)(contents + zk->reloc_ofs), zk->code_size);
    printf("\nRelocations code & data => SWOS data\n");
    rel = WriteRelocs(rel, zk->code_size);
    printf("\nRelocations code & data => SWOS++ load address\n");
    rel = WriteRelocs(rel, zk->code_size);
    printf("\nRelocations patch data => SWOS code\n");
    rel = WriteRelocs(rel, zk->patch_size);
    printf("\nRelocations patch data => SWOS data\n");
    rel = WriteRelocs(rel, zk->patch_size);
    printf("\nRelocations patch data => SWOS++ load address\n");
    WriteRelocs(rel, zk->patch_size);

    fclose(in);
    xfree(contents);

    return 0;
}