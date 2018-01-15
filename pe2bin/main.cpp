#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "pe.h"
#include "zk.h"

#define MAX_FILENAME 256
#define FIXUP_ARRAY_SIZE 6000

#define FALSE 0
#define TRUE  1

#ifndef _MSC_VER
#ifndef __cdecl
#define __cdecl
#endif
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

static dword codeSWOSCodeFixup[FIXUP_ARRAY_SIZE];
static int cscPtr;
static dword codeSWOSDataFixup[FIXUP_ARRAY_SIZE];
static int csdPtr;
static dword codeBaseFixup[FIXUP_ARRAY_SIZE];
static int cbPtr;
static dword patchSWOSCodeFixup[FIXUP_ARRAY_SIZE];
static int pscPtr;
static dword patchSWOSDataFixup[FIXUP_ARRAY_SIZE];
static int psdPtr;
static dword patchBaseFixup[FIXUP_ARRAY_SIZE];
static int pbPtr;

static void push(dword *stack, int *stackptr, dword addr)
{
    if (*stackptr >= FIXUP_ARRAY_SIZE) {
        fprintf(stderr, "Fixup stack overflow! Size: %d\n", FIXUP_ARRAY_SIZE);
        exit(1);
    }

    stack[(*stackptr)++] = addr;
}

static void addCodeSWOSCFixup(dword addr)
{
    push(codeSWOSCodeFixup, &cscPtr, addr);
}

static void addCodeSWOSDFixup(dword addr)
{
    push(codeSWOSDataFixup, &csdPtr, addr);
}

static void addCodeBaseFixup(dword addr)
{
    push(codeBaseFixup, &cbPtr, addr);
}

static void addPatchSWOSCFixup(dword addr)
{
    push(patchSWOSCodeFixup, &pscPtr, addr);
}

static void addPatchSWOSDFixup(dword addr)
{
    push(patchSWOSDataFixup, &psdPtr, addr);
}

static void addPatchBaseFixup(dword addr)
{
    push(patchBaseFixup, &pbPtr, addr);
}

static int checksum(dword c, int i, unsigned char *mem, int memSize)
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

enum RelocSections {
    CODE, DATA, RDATA, PATCH, SWOS, INVALID,
};

struct Section {
    const char *name;
    RelocSections relocIndex;
    unsigned char *buffer;
    dword size;
    dword vsize;
    dword vaddr;
} static sects[] = {
    { ".text", CODE, },
    { ".data", DATA, },
    { ".rdata", RDATA, },
    { ".swos", SWOS, },
    { ".reloc", INVALID, },
    { ".ptdata", PATCH },
};

enum SectionsNeeded {
    SEC_TEXT,
    SEC_DATA,
    SEC_RDATA,
    SEC_SWOS,
    SEC_RELOC,
    SEC_PTDATA
};

/* not so much strict ordering, all we need is to process rdata (exports) before ptdata */
static int sectionOrder[] = { SEC_TEXT, SEC_DATA, SEC_RDATA, SEC_PTDATA, SEC_RELOC, SEC_SWOS };

#define sizeofarray(x) (sizeof(x)/sizeof(x[0]))
#define SECTS_NEEDED sizeofarray(sects)

static void *xmalloc(size_t size)
{
    void *p = malloc(size);

    if (!p) {
        fprintf(stderr, "Out of memory, couldn't fulfill request for %d bytes.\n", size);
        exit(1);
    }

    return p;
}

static bool *getIgnoredSections(const SectionHeader *sectionHeaders, size_t numSections)
{
    /* ignore any fixups belonging to these sections */
    static const char *ignoredSectionNames[] = {
        ".debug_i", ".debug_l", ".debug_r", ".debug_a", ".debug_f"
    };

    bool *ignoredSections = (bool *)xmalloc(numSections * sizeof(bool));

    for (size_t i = 0; i < numSections; i++) {
        char sectionName[9];
        memcpy(sectionName, sectionHeaders[i].name, 8);
        sectionName[8] = '\0';
        ignoredSections[i] = false;
        for (size_t j = 0; j < sizeofarray(ignoredSectionNames); j++) {
            if (!strcmp(ignoredSectionNames[j], sectionName)) {
                ignoredSections[i] = true;
                break;
            }
        }
    }

    return ignoredSections;
}

static bool isIgnoredSection(bool *ignoredSections, dword address, const SectionHeader *sectionHeaders, size_t numSections)
{
    for (size_t i = 0; i < numSections; i++) {
        if (address >= sectionHeaders[i].vaddr && address < sectionHeaders[i].vaddr + sectionHeaders[i].virtualSize)
            return ignoredSections[i];
    }

    return false;
}

static int calculateChecksum(uint rdataOffset, uint rdataSize, uint patchOffset, uint patchSize)
{
    /* relocations sections delimiter */
    const int minusOne = -1;
    int c, size;

    /* calculate checksum */
    c = checksum(0, size = 0, sects[SEC_TEXT].buffer, sects[SEC_TEXT].size);
    c = checksum(c, size += sects[SEC_TEXT].size, sects[SEC_DATA].buffer, sects[SEC_DATA].size);
    c = checksum(c, size += sects[SEC_DATA].size, sects[SEC_RDATA].buffer + rdataOffset, rdataSize);
    c = checksum(c, size += rdataSize, sects[SEC_PTDATA].buffer + patchOffset, patchSize);
    c = checksum(c, size += patchSize, (uchar *)codeSWOSCodeFixup, cscPtr * sizeof(dword));
    c = checksum(c, size += cscPtr * sizeof(dword), (uchar *)&minusOne, sizeof(dword));
    c = checksum(c, size += sizeof(dword), (uchar *)codeSWOSDataFixup, csdPtr * sizeof(dword));
    c = checksum(c, size += csdPtr * sizeof(dword), (uchar *)&minusOne, sizeof(dword));
    c = checksum(c, size += sizeof(dword), (uchar *)codeBaseFixup, cbPtr * sizeof(dword));
    c = checksum(c, size += cbPtr * sizeof(dword),(uchar *)&minusOne, sizeof(dword));
    c = checksum(c, size += sizeof(dword), (uchar *)patchSWOSCodeFixup, pscPtr * sizeof(dword));
    c = checksum(c, size += pscPtr * sizeof(dword), (uchar *)&minusOne,sizeof(dword));
    c = checksum(c, size += sizeof(dword), (uchar *)patchSWOSDataFixup, psdPtr * sizeof(dword));
    c = checksum(c, size += psdPtr * sizeof(dword), (uchar *)&minusOne,sizeof(dword));
    c = checksum(c, size += sizeof(dword), (uchar *)patchBaseFixup, pbPtr * sizeof(dword));
    c = checksum(c, size + pbPtr * sizeof(dword), (uchar *)&minusOne, sizeof(dword));

    return c;
}


static void safefread(void *buffer, size_t size, size_t count, FILE *stream)
{
    if (fread(buffer, size, count, stream) != count && size) {
        fprintf(stderr, "File read error.\nsize = %u count = %u pos = %ld\n", size, count, ftell(stream));
        exit(1);
    }
}

static void safefwrite(const void *buffer, size_t size, size_t count, FILE *stream)
{
    if (fwrite(buffer, size, count, stream) != count && size) {
        fputs("File write error.\n", stderr);
        exit(1);
    }
}

static void safefseek(FILE *stream, long offset, int origin)
{
    if (fseek(stream, offset, origin)) {
        fputs("Seek error.\n", stderr);
        exit(1);
    }
}

static void getDestinationFilename(const char *fileArg, char *dest, size_t destLen, const char *ext)
{
    size_t len = strlen(fileArg);
    strncpy(dest, fileArg, min(len, destLen));
    dest[destLen - 1] = '\0';

    char *dot;
    for (dot = &dest[len - 1]; dot >= dest && *dot != '.'; dot--)
        ;

    if (*dot != '.')
        *(dot = &dest[len - 1]) = '.';

    for (len = destLen - (dot++ - dest) - 1; len > 3; len--)
        if (!(*dot++ = *ext++))
            break;

    if (dest[destLen - 1] == '.')
        dest[destLen - 1] = '\0';
}

static const char *getSectionName(dword value, const SectionHeader *sectionHeaders, size_t numSections)
{
    static char sectionName[9];
    for (size_t i = 0; i < numSections; i++) {
        if (value >= sectionHeaders[i].vaddr && value < sectionHeaders[i].vaddr + sectionHeaders[i].virtualSize) {
            memcpy(sectionName, sectionHeaders[i].name, 8);
            sectionName[8] = '\0';
            return sectionName;
        }
    }

    return "<unknown section>";
}

static int getRelocSectionFixupPointingTo(dword value)
{
    for (size_t i = 0; i < SECTS_NEEDED; i++)
        if (value >= sects[i].vaddr && value < sects[i].vaddr + sects[i].vsize)
            return i;

    return -1;
}

static RelocSections displacementRelocFix(dword value, uchar *fromSectBuf, dword ofs, uint rdataOffset)
{
    /** great troubles here: in MS VC++'s output() function, a table is accessed with 32 bytes
        negative displacement as a result of optimizations; we'll give this fixup to next section
        to compensate for this behaviour; another case is dynamic patch code which has to add negative
        offsets to function addresses */
    uint nextSection = UINT_MAX;
    for (uint minDelta = UINT_MAX, i = 0; i < SECTS_NEEDED; i++) {
        if (sects[i].vaddr > value && sects[i].vaddr < minDelta) {
            nextSection = i;
            minDelta = sects[i].vaddr;
        }
    }

    /* I'll cover only SEC_RDATA and SEC_TEXT for now, and add on a case-by-case basis */
    switch (nextSection) {
    case SEC_TEXT:
        return CODE;
    case SEC_RDATA:
        return RDATA;
    }

    return INVALID;
}

static void reportInvalidFixup(const char *from, const char *to, dword offset)
{
    fprintf(stderr, "Invalid fixup encountered, `%s' -> `%s', offset %u\n", from, to, offset);
    exit(1);
}

static void generateRelocations(const PE_OptionalHeader *peopt, const SectionHeader *sectionHeaders,
    size_t numSections, int *warnings, uint bssOffset, uint rdataOffset, uint rdataSize, uint patchOffset, uint patchSize)
{
    bool *ignoredSections = getIgnoredSections(sectionHeaders, numSections);

    for (word *p = (word *)sects[SEC_RELOC].buffer; ; ) {
        dword pageRVA = *(dword *)p;
        dword numRelocs = (*(dword *)(p + 2) - 8) / 2;

        p += 4;
        if (!((dword *)p)[-1])
            break;

        size_t i;
        for (i = 0; i < SECTS_NEEDED; i++)
            if (pageRVA >= sects[i].vaddr && pageRVA < sects[i].vaddr + sects[i].vsize)
                break;

        if (i == SECTS_NEEDED) {     /* skip this fixup block, it's not ours */
            if (!isIgnoredSection(ignoredSections, pageRVA, sectionHeaders, numSections)) {
                fprintf(stderr, "Warning: skipping fixup block (section `%s', RVA = 0x%08x)\n", getSectionName(pageRVA, sectionHeaders, numSections), pageRVA);
                ++*warnings;
            }
            p += numRelocs;
            continue;
        }

        RelocSections fromSection = sects[i].relocIndex;
        if (fromSection == INVALID || fromSection == SWOS) {
            fprintf(stderr, "Section `%s' is not allowed to have fixups.\n", sects[i].name);
            ++*warnings;
            continue;
        }

        for (size_t j = 0; j < numRelocs; j++, p++) {
            int type = *p >> 12;
            dword value;
            switch (type) {
            default:
                fprintf(stderr, "Unexpected fixup type: %d\n", type);
                exit(1);
            case 0:
                break;
            case 3:
                dword ofs = (*p & 0xfff) + pageRVA - sects[i].vaddr;
                dword *fixupLoc = (dword *)(sects[i].buffer + ofs);
                value = (*fixupLoc -= peopt->imageBase);

                int toSectionIndex = getRelocSectionFixupPointingTo(value);
                RelocSections toSection = toSectionIndex >= 0 ? sects[toSectionIndex].relocIndex : INVALID;

                if (toSection == INVALID && (toSection = displacementRelocFix(value, sects[i].buffer, ofs, rdataOffset)) == INVALID) {
                    fprintf(stderr, "Fixup record pointing to unknown or invalid section `%s' (from section `%s').\n",
                        getSectionName(value, sectionHeaders, numSections), sects[j].name);
                    fprintf(stderr, "value: 0x%08x pageRVA: %08x offset: 0x%08x type = %d ordinal %d, total %d\n",
                        value, pageRVA, ofs, type, j, numRelocs);
                    /* don't end program here; we issued a warning, image probably won't run, but maybe this is some kind of experiment */
                    ++*warnings;
                    break;
                }

                /* from and to sections are now valid, calculate fixups */
                assert(fromSection >= CODE && fromSection <= PATCH && toSection >= CODE && toSection <= SWOS);
                *fixupLoc -= sects[toSectionIndex].vaddr;

                /* bring offsets to correct position, in regard to our final sections layout */
                bool ourRdataItem = false;
                switch (toSection) {
                case CODE:
                    break;

                case DATA:
                    /* offset bss references since it comes between data and rdata */
                    if (*fixupLoc >= bssOffset)
                        *fixupLoc += rdataSize;
                    *fixupLoc += sects[SEC_TEXT].size;
                    break;

                case SWOS:
                    value -= sects[SEC_SWOS].vaddr;
                    if (value >= 0xb0000)
                        *fixupLoc -= 0xb0000;
                    break;

                case RDATA:
                    if (value >= peopt->dataDir[DIR_IAT].vaddr && value < peopt->dataDir[DIR_IAT].vaddr + peopt->dataDir[DIR_IAT].size) {
                        fprintf(stderr, "Unsupported DLL call from section %s, offset 0x%08x.\n", sects[j].name, ofs);
                        /* set it to zero to cause access violation if referenced */
                        *fixupLoc = 0;
                        ++*warnings;
                    }

                    value -= sects[SEC_RDATA].vaddr;
                    if (value >= rdataOffset && value < rdataOffset + rdataSize) {
                        *fixupLoc += sects[SEC_TEXT].size + sects[SEC_DATA].size;
                        ourRdataItem = true;
                    }
                    break;

                case PATCH:
                    if (value >= patchOffset && value < patchOffset + patchSize) {
                        fprintf(stderr, "Fixup record pointing to patch data! This shouldn't happen!\n");
                        exit(1);
                    }

                    fprintf(stderr, "Fixup record pointing to `.ptdata' section, but not to patch data!!!\n");
                    exit(1);

                default:
                    reportInvalidFixup(sects[i].name, sects[toSectionIndex].name, ofs);
                }

                switch (toSection) {
                case CODE:
                case DATA:
                case RDATA:
                    if (toSection == RDATA && !ourRdataItem)
                        break;

                    switch (fromSection) {
                    case RDATA:
                        ofs += sects[SEC_DATA].size - rdataOffset;
                        /* fall-through */

                    case DATA:
                        ofs += sects[SEC_TEXT].size;
                        /* fall-through */

                    case CODE:
                        addCodeBaseFixup(ofs);
                        break;

                    case PATCH:
                        addPatchBaseFixup(ofs);
                        break;

                    default:
                        reportInvalidFixup(sects[i].name, sects[toSectionIndex].name, ofs);
                    }
                    break;

                case SWOS:
                    switch (fromSection) {
                    case RDATA:
                        ofs += sects[SEC_DATA].size - rdataOffset;
                        /* fall-through */

                    case DATA:
                        ofs += sects[SEC_TEXT].size;
                        /* fall-through */

                    case CODE:
                        if (value < 0xb0000)
                            addCodeSWOSCFixup(ofs);
                        else
                            addCodeSWOSDFixup(ofs);
                        break;

                    case PATCH:
                        if (value < 0xb0000)
                            addPatchSWOSCFixup(ofs);
                        else
                            addPatchSWOSDFixup(ofs);
                        break;

                    default:
                        reportInvalidFixup(sects[i].name, sects[toSectionIndex].name, ofs);
                    }
                    break;
                default:
                    reportInvalidFixup(sects[i].name, sects[toSectionIndex].name, ofs);
                }
            }
        }
    }

    free(ignoredSections);
}

int __cdecl main(int argc, char **argv)
{
    char outFile[MAX_FILENAME + 1], *base;
    FILE *in, *out;
    dword i, j, peOffset, *functions, *names, rptr, patchOffset;
    word *ordinals;
    PE_header peh;
    PE_OptionalHeader peopt;
    SectionHeader *sectionHeaders;
    int bssSize, bssOffset, entryPoint = -1, patchSize, gotPatchSize, gotPatchStart;
    ZK_header zk;
    ExportDirectoryTable *edt;
    ImportDirectoryTable *idt;
    int warnings = 0;
    uint rdataOffset = 0, rdataSize = 0;
    uint codeDataPad, patchPad;

    assert(sizeofarray(sects) == sizeofarray(sectionOrder));

    fputs("pe2bin v1.2 - converter from Win32 PE exe to Zlatko Karakas Binary Format\n"
          "Copyright Zlatko Karakas 2003-2017.\n\n", stderr);

    if (argc < 2) {
        fputs("Input filename missing.\n", stderr);
        return 1;
    }

    getDestinationFilename(argv[1], outFile, sizeof(outFile), "bin");

    if (!(in = fopen(argv[1], "rb"))) {
        fprintf(stderr, "Can't open file %s.\n", outFile);
        return 1;
    }

    if (fseek(in, 0x3c, SEEK_SET) ||
        fread(&peOffset, sizeof(dword), 1, in) != 1 ||
        fseek(in, peOffset, SEEK_SET) ||
        fread(&peh, sizeof(PE_header), 1, in) != 1 ||
        peh.signature != PE_SIGNATURE ||
        fread(&peopt, sizeof(PE_OptionalHeader), 1, in) != 1) {
        fprintf(stderr, "%s is not a PE executable.\n", outFile);
        return 1;
    }

    if (peopt.sectionAlignment > 4096) {
        fputs("Warning: Section alignment greater than 4096.\n", stderr);
        warnings++;
    }

    if (peopt.fileAlignment > 512) {
        fputs("Warning: File alignment greater than 512.\n", stderr);
        warnings++;
    }

    /* check for unknown directories */
    for (i = 0; i < peopt.numOf_RVA_AndSizes; i++) {
        if (i == DIR_IMPORT || i == DIR_EXPORT || i == DIR_BASERELOC || i == DIR_IAT)
            continue;
        if (peopt.dataDir[i].vaddr | peopt.dataDir[i].size) {
            fprintf(stderr, "Unknown data directory found. (%d)\n", i);
            return 1;
        }
    }

    /* allocate memory and read in all section headers */
    if (peh.numSections < sizeofarray(sects)) {
        fprintf(stderr, "Required sections missing. Executable only has %d (required at least %u).\n",
            peh.numSections, sizeofarray(sects));
        return 1;
    }

    sectionHeaders = (SectionHeader *)xmalloc(peh.numSections * sizeof(SectionHeader));
    safefread(sectionHeaders, peh.numSections * sizeof(SectionHeader), 1, in);

    /* load and process sections in predetermined order */
    for (i = 0; i < sizeofarray(sectionOrder); i++) {
        const SectionHeader *sec;
        for (j = 0; j < peh.numSections; j++)
            if (!strncmp((const char *)sectionHeaders[j].name, sects[sectionOrder[i]].name, strlen(sects[sectionOrder[i]].name)))
                break;

        if (j >= peh.numSections) {
            fprintf(stderr, "Required section %s is missing.\n", sects[sectionOrder[i]].name);
            return 1;
        }

        sec = sectionHeaders + j;
        sects[sectionOrder[i]].vaddr = sec->vaddr;
        sects[sectionOrder[i]].vsize = sec->virtualSize;
        sects[sectionOrder[i]].size = min(sec->sizeOfRawData, sec->virtualSize);

        /* round up text size on base 4 because of data alignment */
        if (j == SEC_TEXT)
            sects[sectionOrder[i]].size = (sects[sectionOrder[i]].size + 3) & ~3;

        /* load it up */
        if (sec->sizeOfRawData) {
            safefseek(in, sec->ptrToRawData, SEEK_SET);
            sects[sectionOrder[i]].buffer = (unsigned char *)xmalloc(sects[sectionOrder[i]].size);
            safefread(sects[sectionOrder[i]].buffer, sects[sectionOrder[i]].size, 1, in);
        }

        /* and finally go on and process it */
        switch (sectionOrder[i]) {
        case SEC_TEXT:  /* entry point MUST be in .text section */
            entryPoint = peopt.entryPoint - sec->vaddr;
            break;

        case SEC_DATA:  /* expecting .bss to be merged with .data */
            bssSize = sec->virtualSize - sec->sizeOfRawData;
            bssOffset = sec->sizeOfRawData;
            bssSize = max(0, bssSize);
            break;

        case SEC_RDATA:
            if (peopt.dataDir[DIR_EXPORT].vaddr < sec->vaddr ||
                peopt.dataDir[DIR_EXPORT].vaddr + peopt.dataDir[DIR_EXPORT].size > sec->vaddr + sec->virtualSize) {
                fputs("Export directory must be in `.rdata' section.\n", stderr);
                return 1;
            }

            base = (char *)sects[SEC_RDATA].buffer;
            edt = (ExportDirectoryTable *)(base + peopt.dataDir[DIR_EXPORT].vaddr - sec->vaddr);
            functions = (dword *)(base + edt->exportAddrTableRVA - sec->vaddr);
            ordinals = (word *)(base + edt->ordinalTableRVA - sec->vaddr);
            names = (dword *)(base + edt->namePointerRVA - sec->vaddr);
            gotPatchSize = gotPatchStart = FALSE;

            for (j = 0; j < edt->addrTableEntries; j++) {
                dword entryPointRVA = functions[j], i;

                if (!entryPointRVA)
                    continue;

                for (i = 0; i < edt->numNamePointers; i++) {
                    if (ordinals[i] == j) {
                        if (!strcmp(base + names[i] - sec->vaddr, "PatchSize")) {
                            gotPatchSize = TRUE;
                            patchSize = entryPointRVA;
                            break;
                        } else if (!strcmp(base + names[i] - sec->vaddr, "PatchStart")) {
                            gotPatchStart = TRUE;
                            patchOffset = entryPointRVA;
                            break;
                        }
                    }
                }
            }

            if (!gotPatchSize) {
                fputs("Could not find exported symbol `PatchSize'\n", stderr);
                return 1;
            }

            if (!gotPatchStart) {
                fputs("Could not find exported symbol `PatchStart'\n", stderr);
                return 1;
            }

            /* calculate exact size of import section (if it exists) */
            if (peopt.dataDir[DIR_IMPORT].size) {
                int size = 0;
                uchar *p, *next = (uchar *)base;
                dword *d;
                idt = (ImportDirectoryTable *)(next + peopt.dataDir[DIR_IMPORT].vaddr - sec->vaddr);
                for (; idt->lookupTableRVA; idt++) {
                    if (idt->forwarderChain) {
                        fprintf(stderr, "Forwarder chains not supported.\n");
                        return 1;
                    }

                    p = (uchar *)(base + idt->nameRVA - sec->vaddr);

                    while (*p++)
                        size++;

                    size++;
                    d = (dword *)(base + idt->lookupTableRVA - sec->vaddr);

                    for (; *d; d++) {
                        if (!(*d & ~INT_MAX)) {
                            p = (uchar *)(base + (*d & INT_MAX) - sec->vaddr);
                            p += 2, size += 2;
                            while (*p++)
                                size++;

                            size++;
                            size += (uint)p & 1;
                        }
                        if (idt->iatRVA != idt->lookupTableRVA)
                            size += 4;
                    }
                }
                peopt.dataDir[DIR_IMPORT].size += size;
            }

            /* find out rdata offset and size */
            rptr = sec->vaddr;
            for (j = 0; j < 3; j++) {
                if (peopt.dataDir[DIR_IMPORT].vaddr == rptr)
                    rptr += peopt.dataDir[DIR_IMPORT].size;
                if (peopt.dataDir[DIR_EXPORT].vaddr == rptr)
                    rptr += peopt.dataDir[DIR_EXPORT].size;
                if (peopt.dataDir[DIR_IAT].vaddr == rptr)
                    rptr += peopt.dataDir[DIR_IAT].size;
            }

            rdataOffset = rptr;
            rptr = sec->vaddr + sects[SEC_RDATA].size;
            if (rdataOffset < peopt.dataDir[DIR_IMPORT].vaddr)
                rptr = peopt.dataDir[DIR_IMPORT].vaddr;

            if (rdataOffset < peopt.dataDir[DIR_EXPORT].vaddr)
                rptr = min(rptr, peopt.dataDir[DIR_EXPORT].vaddr);

            if (rdataOffset < peopt.dataDir[DIR_IAT].vaddr)
                rptr = min(rptr, peopt.dataDir[DIR_IAT].vaddr);

            rdataSize = rptr - rdataOffset;
            rdataOffset -= sec->vaddr;
            break;

        case SEC_PTDATA:
            patchSize = *(dword *)(sects[SEC_PTDATA].buffer + patchSize - sec->vaddr);
            patchOffset -= sec->vaddr;
            if (patchSize < 6) {
                fprintf(stderr, "Patch size invalid! (%d)\n", patchSize);
                exit(1);
            }
            break;

        case SEC_SWOS:
            if (sec->sizeOfRawData) {
                fputs("Section .swos contains actual data/code!\n", stderr);
                exit(1);
            }
            break;
        }
    }

    if (!peopt.dataDir[DIR_BASERELOC].vaddr || !peopt.dataDir[DIR_BASERELOC].size) {
        fputs("Executable missing base relocations.\n", stderr);
        return 1;
    }

    if (peopt.dataDir[DIR_BASERELOC].vaddr != sects[SEC_RELOC].vaddr) {
        fputs(".reloc section must contain base relocations.\n", stderr);
        return 1;
    }

    if (entryPoint < 0) {
        fputs("Entry point must be in .text section.\n", stderr);
        return 1;
    }

    /* we got everything we needed from the input */
    fclose(in);

    generateRelocations(&peopt, sectionHeaders, peh.numSections, &warnings, bssOffset, rdataOffset, rdataSize, patchOffset, patchSize);

    int c = calculateChecksum(rdataOffset, rdataSize, patchOffset, patchSize);

    /* do it portably :P */
    zk.signature[0] = 'Z';
    zk.signature[1] = 'K';
    zk.signature[2] = 'B';
    zk.signature[3] = 'F';
    zk.verMajor = 1;
    zk.verMinor = 0;
    zk.headerSize = sizeof(ZK_header);
    zk.checksum = c;
    zk.codeOffset = sizeof(ZK_header);
    zk.codeSize = sects[SEC_TEXT].size + sects[SEC_DATA].size + rdataSize;
    codeDataPad = (4 - (zk.codeSize & 3)) & 3;
    zk.patchOffset = zk.codeOffset + zk.codeSize + codeDataPad;
    patchPad = (4 - (patchSize & 3)) & 3;
    zk.patchSize = patchSize;
    zk.relocOffset = zk.patchOffset + zk.patchSize + patchPad;
    zk.relocSize = sizeof(dword) * cscPtr + sizeof(dword) * csdPtr + sizeof(dword) * cbPtr +
        sizeof(dword) * pscPtr + sizeof(dword) * psdPtr + sizeof(dword) * pbPtr + 6 * sizeof(dword); // + 6 delimiters
    zk.bssSize = bssSize;
    zk.entryPoint = peopt.entryPoint - sects[SEC_TEXT].vaddr;

    if (!(out = fopen(outFile, "wb"))) {
        fprintf(stderr, "Can't open %s for writing\n", outFile);
        return 1;
    }

    /* write out bin file */
    const int minusOne = -1;
    safefwrite(&zk, sizeof(ZK_header), 1, out);
    safefwrite(sects[SEC_TEXT].buffer, sects[SEC_TEXT].size, 1, out);
    safefwrite(sects[SEC_DATA].buffer, sects[SEC_DATA].size, 1, out);
    safefwrite(sects[SEC_RDATA].buffer + rdataOffset, rdataSize, 1, out);
    safefwrite("\0\0", codeDataPad, 1, out);
    safefwrite(sects[SEC_PTDATA].buffer + patchOffset, patchSize, 1, out);
    safefwrite("\0\0", patchPad, 1, out);
    safefwrite(codeSWOSCodeFixup, cscPtr * sizeof(dword), 1, out);
    safefwrite(&minusOne, sizeof(dword), 1, out);
    safefwrite(codeSWOSDataFixup, csdPtr * sizeof(dword), 1 ,out);
    safefwrite(&minusOne, sizeof(dword), 1, out);
    safefwrite(codeBaseFixup, cbPtr * sizeof(dword), 1, out);
    safefwrite(&minusOne, sizeof(dword), 1, out);
    safefwrite(patchSWOSCodeFixup, pscPtr * sizeof(dword), 1, out);
    safefwrite(&minusOne, sizeof(dword), 1, out);
    safefwrite(patchSWOSDataFixup, psdPtr * sizeof(dword), 1, out);
    safefwrite(&minusOne, sizeof(dword), 1, out);
    safefwrite(patchBaseFixup, pbPtr * sizeof(dword), 1, out);
    safefwrite(&minusOne, sizeof(dword), 1, out);

    /* free sections */
    for (size_t i = 0; i < SECTS_NEEDED; i++) {
        if (sects[i].buffer) {
            free(sects[i].buffer);
            sects[i].buffer = NULL;
        }
    }

    fclose(out);

    if (warnings > 0)
        fprintf(stderr, "%d warning(s). Image may not run.\n", warnings);
    else
        fputs("All OK.\n", stderr);

    fprintf(stderr, "Memory required for loading: %d bytes\n", zk.codeSize + zk.bssSize + zk.patchSize + zk.relocSize);

    return 0;
}
