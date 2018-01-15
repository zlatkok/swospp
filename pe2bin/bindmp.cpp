/* bindmp - utility to dump ZK binary format, useful for validating binary file */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pe.h"
#include "zk.h"

static int Checksum(dword c, int i, unsigned char *mem, int memSize)
{
    unsigned int t;
    for (int j = 0; j < memSize; mem++, j++, i++) {
        c += i + *mem;
        t = c & 0x80000000;
        c <<= 1;
        c |= t >> 31;
        c ^= i + *mem;
    }
    return c;
}

static void saferead(char *fname, void *buf, size_t size, size_t cnt, FILE *f)
{
    if (fread(buf, size, cnt, f) != cnt && size) {
        fprintf(stderr, "Read error in file %s.\nsize = %d count = %d pos = %ld", fname, size, cnt, ftell(f));
        exit(1);
    }
}

static void safeseek(char *fname, FILE *f, long ofs, int origin)
{
    if (fseek(f, ofs, origin)) {
        fprintf(stderr, "Seek error in file %s.\npos = %ld origin = %d\n", fname, ofs, origin);
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
    uchar *saved;
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

static const dword *WriteRelocs(const dword *p, const dword *sentinel, const uchar *sectionData, int limit, bool *valid)
{
    for (; p < sentinel && *p != (dword)-1; p++) {
        if ((int)*p > limit - 4) {
            printf("<< INVALID RELOCATION >> 0x%08x > 0x%08x\n", *p, limit);
            *valid = false;
        } else {
            printf("0x%08x [%08x]\n", *p, *(dword *)(sectionData + *p));
        }
    }
    if (p >= sentinel) {
        printf("Invalid relocation section, terminator is missing!");
        exit(1);
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
    fprintf(stderr, "bindmp - utility for dumping ZK binary files\n");
    fprintf(stderr, "Copyright Zlatko Karakas 2003, 2017.\n\n");

    if (argc < 2) {
        fprintf(stderr, "Input filename missing.\n");
        return 1;
    }

    FILE *in;
    if (!(in = fopen(argv[1], "rb"))) {
        fprintf(stderr, "Can't open %s.\n", argv[1]);
        return 1;
    }

    safeseek(argv[1], in, 0, SEEK_END);
    int fileSize = ftell(in);
    rewind(in);

    uchar *contents = (uchar*)xmalloc(fileSize);
    saferead(argv[1], contents, fileSize, 1, in);

    ZK_header *zk = (ZK_header*)contents;

    if (strncmp(zk->signature, "ZKBF", 4)) {
        fprintf(stderr, "Invalid ZK signature: %4s\n", zk->signature);
        return 1;
    }

    int size;
    int c = Checksum(0, size = 0, contents + zk->codeOffset, zk->codeSize);
    c = Checksum(c, size += zk->codeSize, contents + zk->patchOffset, zk->patchSize);
    c = Checksum(c, size += zk->patchSize, contents + zk->relocOffset, zk->relocSize);

    /* print header */
    printf("[%s]\n\n", argv[1]);
    printf("File size:             %d\n", fileSize);
    printf("Signature:             %4.4s\n", zk->signature);
    printf("Version:               %d.%d\n", zk->verMajor, zk->verMinor);
    printf("Header size:           %d\n", zk->headerSize);
    printf("Checksum:              %u (%u)\n", zk->checksum, c);
    printf("Offset to code:        %d\n", zk->codeOffset);
    printf("Size of code:          %d\n", zk->codeSize);
    printf("Offset to patch data:  %d\n", zk->patchOffset);
    printf("Size of patch data:    %d\n", zk->patchSize);
    printf("Offset to relocations: %d\n", zk->relocOffset);
    printf("Size of relocations:   %d\n", zk->relocSize);
    printf("BSS size:              %d\n", zk->bssSize);
    printf("Entry point:           %d\n", zk->entryPoint);

    putchar('\n');
    putchar('\n');

    printf("Code and data section hex dump:\n\n");
    HexDump(contents + zk->codeOffset, zk->codeSize);

    printf("\nPatch data:\n\n");
    WritePatchData(contents + zk->patchOffset, zk->patchSize);

    dword *sentinel = (dword *)(contents + fileSize);
    bool valid = true;
    printf("\nRelocations code & data => SWOS code\n");
    const dword *rel = WriteRelocs((dword *)(contents + zk->relocOffset), sentinel, contents + zk->codeOffset, zk->codeSize, &valid);
    printf("\nRelocations code & data => SWOS data\n");
    rel = WriteRelocs(rel, sentinel, contents + zk->codeOffset, zk->codeSize, &valid);
    printf("\nRelocations code & data => SWOS++ load address\n");
    rel = WriteRelocs(rel, sentinel, contents + zk->codeOffset, zk->codeSize, &valid);
    printf("\nRelocations patch data => SWOS code\n");
    rel = WriteRelocs(rel, sentinel, contents + zk->patchOffset, zk->patchSize, &valid);
    printf("\nRelocations patch data => SWOS data\n");
    rel = WriteRelocs(rel, sentinel, contents + zk->patchOffset, zk->patchSize, &valid);
    printf("\nRelocations patch data => SWOS++ load address\n");
    WriteRelocs(rel, sentinel, contents + zk->patchOffset, zk->patchSize, &valid);

    fclose(in);
    xfree(contents);

    return !valid;
}
