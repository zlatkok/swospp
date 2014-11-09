#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "pe.h"
#include "zk.h"

#define MAX_FILENAME 256
#define FIXUP_ARRAY_SIZE 5000

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

static dword code_swosc_fixup[FIXUP_ARRAY_SIZE];
static int csc_ptr;
static dword code_swosd_fixup[FIXUP_ARRAY_SIZE];
static int csd_ptr;
static dword code_base_fixup[FIXUP_ARRAY_SIZE];
static int cb_ptr;
static dword patch_swosc_fixup[FIXUP_ARRAY_SIZE];
static int psc_ptr;
static dword patch_swosd_fixup[FIXUP_ARRAY_SIZE];
static int psd_ptr;
static dword patch_base_fixup[FIXUP_ARRAY_SIZE];
static int pb_ptr;

static void push(dword *stack, int *stackptr, dword addr)
{
    if (*stackptr >= FIXUP_ARRAY_SIZE) {
        fputs("Fixup stack overflow!", stderr);
        exit(1);
    }
    stack[(*stackptr)++] = addr;
}

static void AddCodeSWOSCFixup(dword addr)
{
    push(code_swosc_fixup, &csc_ptr, addr);
}

static void AddCodeSWOSDFixup(dword addr)
{
    push(code_swosd_fixup, &csd_ptr, addr);
}

static void AddCodeBaseFixup(dword addr)
{
    push(code_base_fixup, &cb_ptr, addr);
}

static void AddPatchSWOSCFixup(dword addr)
{
    push(patch_swosc_fixup, &psc_ptr, addr);
}

static void AddPatchSWOSDFixup(dword addr)
{
    push(patch_swosd_fixup, &psd_ptr, addr);
}

static void AddPatchBaseFixup(dword addr)
{
    push(patch_base_fixup, &pb_ptr, addr);
}

static int Checksum(dword c, int i, unsigned char *mem, int mem_size)
{
    unsigned int j, t;
    for (j = 0; j < mem_size; mem++, j++, i++) {
        c += i + *mem;
        t = c & 0x80000000;
        c <<= 1;
        c |= t >> 31;
        c ^= i + *mem;
    }
    return c;
}

struct Section {
    char  *name;
    unsigned char *buffer;
    dword size;
    dword vsize;
    dword vaddr;
} static sects[] = {
    ".text", 0, 0, 0, 0,
    ".data", 0, 0, 0, 0,
    ".rdata", 0, 0, 0, 0,
    ".swos", 0, 0, 0, 0,
    ".reloc", 0, 0, 0, 0,
    ".ptdata", 0, 0, 0, 0
};

enum sections_needed {
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
#define sects_needed sizeofarray(sects)

static void safefread(void *buffer, size_t size, size_t count, FILE *stream)
{
    if (fread(buffer, size, count, stream) != count && size) {
        fprintf(stderr, "File read error.\nsize = %d count = %d ", size, count);
        fprintf(stderr, "pos %d\n", ftell(stream));
        exit(1);
    }
}

static void safefwrite(const void *buffer, size_t size, size_t count, FILE *stream)
{
    if (fwrite(buffer, size, count, stream) != count && size) {
        fputs("File write error.", stderr);
        exit(1);
    }
}

static void safefseek(FILE *stream, long offset, int origin)
{
    if (fseek(stream, offset, origin)) {
        fputs("Seek error.", stderr);
        exit(1);
    }
}

static void *xmalloc(size_t size)
{
    void *p;
    if (!(p = malloc(size))) {
        fputs("Out of memory.", stderr);
        exit(1);
    }
    return p;
}

int __cdecl main(int argc, char **argv)
{
    char out_file[MAX_FILENAME + 1], *dot, ext[] = "bin", *pext = ext, *base;
    int len, sects_read = 0;
    FILE *in, *out;
    dword i, j, k, pe_ofs, *functions, *names, c, size, rptr, patch_offset;
    word *p, *ordinals;
    PE_header peh;
    PE_optional_header peopt;
    Section_header *sectionHeaders;
    int bss_size, entry_point, patch_size, got_patch_size, got_patch_start;
    ZK_header zk;
    Export_directory_table *edt;
    Import_directory_table *idt;
    int warnings = 0;
    uint rdata_ofs = 0, rdata_size = 0, next_sec, min_delta;
    uint code_data_pad, patch_pad;

    fputs("pe2bin v1.1 - converter from win32 PE exe to Zlatko Karakas Binary Format\n"
          "Copyright Zlatko Karakas 2003-2014.\n\n", stderr);

    if (argc < 2) {
        fputs("Input filename missing.\n", stderr);
        return 1;
    }

    len = strlen(argv[1]);
    strncpy(out_file, argv[1], min(len, MAX_FILENAME));
    for (dot = &out_file[len - 1]; dot >= out_file && *dot != '.'; dot--);
    if (*dot != '.')
        *(dot = &out_file[len - 1]) = '.';
    for (len = MAX_FILENAME - (dot++ - out_file) - 1; len > 3; len--)
        if (!(*dot++ = *pext++))
            break;

    out_file[MAX_FILENAME] = '\0';
    if (out_file[MAX_FILENAME - 1] == '.')
        out_file[MAX_FILENAME - 1] = '\0';

    if (!(in = fopen(argv[1], "rb"))) {
        fprintf(stderr, "Can't open file %s.\n", out_file);
        return 1;
    }

    if (fseek(in, 0x3c, SEEK_SET) ||
        fread(&pe_ofs, sizeof(dword), 1, in) != 1 ||
        fseek(in, pe_ofs, SEEK_SET) ||
        fread(&peh, sizeof(PE_header), 1, in) != 1 ||
        peh.signature != PE_SIGNATURE ||
        fread(&peopt, sizeof(PE_optional_header), 1, in) != 1) {
        fprintf(stderr, "%s is not a PE executable.\n", out_file);
        return 1;
    }

    if (peopt.section_alignment > 4096) {
        fputs("Warning: Section alignment greater than 4096.\n", stderr);
        warnings++;
    }

    if (peopt.file_alignment > 512) {
        fputs("Warning: File alignment greater than 512.\n", stderr);
        warnings++;
    }

    /* check for unknown directories */
    for (i = 0; i < peopt.number_of_RVA_and_sizes; i++) {
        if (i == DIR_IMPORT || i == DIR_EXPORT || i == DIR_BASERELOC || i == DIR_IAT)
            continue;
        if (peopt.data_dir[i].vaddr | peopt.data_dir[i].size) {
            fprintf(stderr, "Unknown data directory found. (%d)\n", i);
            return 1;
        }
    }

    /* allocate memory and read in all section headers */
    if (peh.num_sections < sizeofarray(sects)) {
        fprintf(stderr, "Required sections missing. Executable only has %d (required at least %d).\n", peh.num_sections, sizeofarray(sects));
        return 1;
    }
    sectionHeaders = (Section_header *)xmalloc(peh.num_sections * sizeof(Section_header));
    safefread(sectionHeaders, peh.num_sections * sizeof(Section_header), 1, in);
    /* load and process sections in predetermined order */
    for (i = 0; i < sizeofarray(sectionOrder); i++) {
        Section_header *sec;
        for (j = 0; j < sizeofarray(sects); j++)
            if (!strncmp((const char *)sectionHeaders[j].name, sects[sectionOrder[i]].name, strlen(sects[sectionOrder[i]].name)))
                break;
        if (j >= sizeofarray(sects)) {
            fprintf(stderr, "Required section %s is missing.\n", sects[sectionOrder[i]]);
            return 1;
        }
        sec = sectionHeaders + j;
        sects[sectionOrder[i]].vaddr = sec->vaddr;
        sects[sectionOrder[i]].vsize = sec->virtual_size;
        sects[sectionOrder[i]].size = min(sec->sizeof_raw_data, sec->virtual_size);
        /* round up text size on base 4 because of data alignment */
        if (j == SEC_TEXT)
            sects[sectionOrder[i]].size = sects[sectionOrder[i]].size + 3 & ~3;
        /* load it up */
        if (sec->sizeof_raw_data) {
            safefseek(in, sec->ptr_to_raw_data, SEEK_SET);
            sects[sectionOrder[i]].buffer = (unsigned char *)xmalloc(sects[sectionOrder[i]].size);
            safefread(sects[sectionOrder[i]].buffer, sects[sectionOrder[i]].size, 1, in);
        }
        /* and finally go on and process it */
        switch (sectionOrder[i]) {
        case SEC_TEXT:  /* entry point MUST be in .text section */
            entry_point = peopt.entry_point - sec->vaddr;
            break;
        case SEC_DATA:  /* expecting .bss to be merged with .data */
            bss_size = sec->virtual_size - sec->sizeof_raw_data;
            bss_size = max(0, bss_size);
            break;
        case SEC_RDATA:
            if (peopt.data_dir[DIR_EXPORT].vaddr < sec->vaddr ||
                peopt.data_dir[DIR_EXPORT].vaddr + peopt.data_dir[DIR_EXPORT].size > sec->vaddr + sec->virtual_size) {
                fputs("Export directory must be in `.rdata' section.\n", stderr);
                return 1;
            }
            base = (char *)sects[SEC_RDATA].buffer;
            edt = (Export_directory_table *)(base + peopt.data_dir[DIR_EXPORT].vaddr - sec->vaddr);
            functions = (dword *)(base + edt->export_addr_tbl_rva - sec->vaddr);
            ordinals = (word *)(base + edt->ordinal_table_rva - sec->vaddr);
            names = (dword *)(base + edt->name_pointer_rva - sec->vaddr);
            got_patch_size = got_patch_start = FALSE;
            for (j = 0; j < edt->addr_tbl_entries; j++) {
                dword entry_point_rva = functions[j], i;

                if (!entry_point_rva)
                    continue;

                for (i = 0; i < edt->num_name_pointers; i++) {
                    if (ordinals[i] == j) {
                        if (!strcmp(base + names[i] - sec->vaddr, "PatchSize")) {
                            got_patch_size = TRUE;
                            patch_size = entry_point_rva;
                            break;
                        } else if (!strcmp(base + names[i] - sec->vaddr, "PatchStart")) {
                            got_patch_start = TRUE;
                            patch_offset = entry_point_rva;
                            break;
                        }
                    }
                }
            }
            if (!got_patch_size) {
                fputs("Could not find exported symbol `PatchSize'\n", stderr);
                return 1;
            }
            if (!got_patch_start) {
                fputs("Could not find exported symbol `PatchStart'\n", stderr);
                return 1;
            }
            /* calculate exact size of import section (if exists) */
            if (peopt.data_dir[DIR_IMPORT].size) {
                int size = 0;
                uchar *p, *next = (uchar *)base;
                dword *d;
                idt = (Import_directory_table *)(next + peopt.data_dir[DIR_IMPORT].vaddr - sec->vaddr);
                for (; idt->lookup_table_rva; idt++) {
                    if (idt->forwarder_chain) {
                        fprintf(stderr, "Forwarder chains not supported.\n");
                        return 1;
                    }
                    p = (uchar *)(base + idt->name_rva - sec->vaddr);
                    while (*p++)
                        size++;
                    size++;
                    d = (dword *)(base + idt->lookup_table_rva - sec->vaddr);
                    for (; *d; d++) {
                        if (!(*d & ~INT_MAX)) {
                            p = (uchar *)(base + (*d & INT_MAX) - sec->vaddr);
                            p += 2, size += 2;
                            while (*p++)
                                size++;
                            size++;
                            size += (uint)p & 1;
                        }
                        if (idt->iat_rva != idt->lookup_table_rva)
                            size += 4;
                    }
                }
                peopt.data_dir[DIR_IMPORT].size += size;
            }

            /* find out rdata offset and size */
            rptr = sec->vaddr;
            for (j = 0; j < 3; j++) {
                if (peopt.data_dir[DIR_IMPORT].vaddr == rptr)
                    rptr += peopt.data_dir[DIR_IMPORT].size;
                if (peopt.data_dir[DIR_EXPORT].vaddr == rptr)
                    rptr += peopt.data_dir[DIR_EXPORT].size;
                if (peopt.data_dir[DIR_IAT].vaddr == rptr)
                    rptr += peopt.data_dir[DIR_IAT].size;
            }
            rdata_ofs = rptr;
            rptr = sec->vaddr + sects[SEC_RDATA].size;
            if (rdata_ofs < peopt.data_dir[DIR_IMPORT].vaddr)
                rptr = peopt.data_dir[DIR_IMPORT].vaddr;
            if (rdata_ofs < peopt.data_dir[DIR_EXPORT].vaddr)
                rptr = min(rptr, peopt.data_dir[DIR_EXPORT].vaddr);
            if (rdata_ofs < peopt.data_dir[DIR_IAT].vaddr)
                rptr = min(rptr, peopt.data_dir[DIR_IAT].vaddr);
            rdata_size = rptr - rdata_ofs;
            rdata_ofs -= sec->vaddr;
            break;
        case SEC_PTDATA:
            patch_size = *(dword *)(sects[SEC_PTDATA].buffer + patch_size - sec->vaddr);
            patch_offset -= sec->vaddr;
            if (patch_size < 6) {
                fprintf(stderr, "Patch size invalid! (%d)\n", patch_size);
                exit(1);
            }
            break;
        }
    }

    if (!peopt.data_dir[DIR_BASERELOC].vaddr || !peopt.data_dir[DIR_BASERELOC].size) {
        fputs("Executable missing base relocations.\n", stderr);
        return 1;
    }

    if (peopt.data_dir[DIR_BASERELOC].vaddr != sects[SEC_RELOC].vaddr) {
        fputs(".reloc section must contain base relocations.\n", stderr);
        return 1;
    }

    for (p = (word *)sects[SEC_RELOC].buffer; ; ) {
        dword page_rva = *(dword *)p, ofs;
        dword num_relocs = (*(dword *)(p + 2) - 8) / 2;

        p += 4;
        if (!((dword *)p)[-1])
            break;
        for (j = 0; j < sects_needed; j++)
            if (page_rva >= sects[j].vaddr && page_rva < sects[j].vaddr + sects[j].vsize)
                break;
        if (j == sects_needed) {     /* skip this fixup block, it's not ours */
            fprintf(stderr, "Warning: Unknown fixup block (RVA = 0x%08x)\n", page_rva);
            warnings++;
            p += num_relocs;
            continue;
        }
        for (i = 0; i < num_relocs; i++, p++) {
            int type = *p >> 12;
            dword value;
            switch (type) {
            case 0:
                break;
            case 3:
                ofs = (*p & 0xfff) + page_rva - sects[j].vaddr;
                value = (*(dword *)(sects[j].buffer + ofs) -= peopt.image_base);
                if (value >= sects[SEC_TEXT].vaddr && value < sects[SEC_TEXT].vaddr + sects[SEC_TEXT].vsize) {
                    *(dword *)(sects[j].buffer + ofs) -= sects[SEC_TEXT].vaddr;
                    if (j == SEC_TEXT)
                        AddCodeBaseFixup(ofs);
                    else if (j == SEC_DATA)
                        AddCodeBaseFixup(ofs + sects[SEC_TEXT].size);
                    else if (j == SEC_PTDATA) {
                        AddPatchBaseFixup(ofs);
                    } else if (j == SEC_RDATA) {
                        AddCodeBaseFixup(ofs + sects[SEC_TEXT].size + sects[SEC_DATA].size - rdata_ofs);
                    } else {
                        fputs("Fixup referencing invalid section.\n", stderr);
                        return 1;
                    }
                } else if (value >= sects[SEC_DATA].vaddr &&
                    value < sects[SEC_DATA].vaddr + sects[SEC_DATA].vsize) {
                    *(dword *)(sects[j].buffer + ofs) -= sects[SEC_DATA].vaddr;
                    *(dword *)(sects[j].buffer + ofs) += sects[SEC_TEXT].size;
                    if (j == SEC_TEXT)
                        AddCodeBaseFixup(ofs);
                    else if (j == SEC_DATA)
                        AddCodeBaseFixup(ofs + sects[SEC_TEXT].size);
                    else if (j == SEC_PTDATA)
                        AddPatchBaseFixup(ofs);
                    else if (j == SEC_RDATA)
                        AddCodeBaseFixup(ofs + sects[SEC_TEXT].size + sects[SEC_DATA].size - rdata_ofs);
                    else {
                        fputs("Fixup referencing invalid section.\n", stderr);
                        return 1;
                    }
                } else if (value >= sects[SEC_SWOS].vaddr && value < sects[SEC_SWOS].vaddr + sects[SEC_SWOS].vsize) {
                    *(dword *)(sects[j].buffer + ofs) -= sects[SEC_SWOS].vaddr;
                    value -= sects[SEC_SWOS].vaddr;
                    if (value >= 0xb0000)
                        *(dword *)(sects[j].buffer + ofs) -= 0xb0000;
                    if (j == SEC_TEXT)
                        if (value < 0xb0000)
                            AddCodeSWOSCFixup(ofs);
                        else
                            AddCodeSWOSDFixup(ofs);
                    else if (j == SEC_DATA)
                        if (value < 0xb0000)
                            AddCodeSWOSCFixup(ofs + sects[SEC_TEXT].size);
                        else
                            AddCodeSWOSDFixup(ofs + sects[SEC_TEXT].size);
                    else if (j == SEC_PTDATA) {
                        if (value < 0xb0000)
                            AddPatchSWOSCFixup(ofs);
                        else
                            AddPatchSWOSDFixup(ofs);
                    } else {
                        fputs("Fixup referencing invalid section.\n", stderr);
                        return 1;
                    }
                } else if (value >= sects[SEC_RDATA].vaddr && value < sects[SEC_RDATA].vaddr + sects[SEC_RDATA].size) {
                    *(dword *)(sects[j].buffer + ofs) -= sects[SEC_RDATA].vaddr;
                    if (value >= peopt.data_dir[DIR_IAT].vaddr && value < peopt.data_dir[DIR_IAT].vaddr + peopt.data_dir[DIR_IAT].size) {
                        fprintf(stderr, "Unsupported call to WinAPI from section %s, offset 0x%08x.\n", sects[j].name, ofs);
                        /* set it to zero to cause access violation if referenced */
                        *(dword *)(sects[j].buffer + ofs) = 0;
                        warnings++;
                    }
                    value -= sects[SEC_RDATA].vaddr;
                    if (value >= rdata_ofs && value < rdata_ofs + rdata_size) {
                        *(dword *)(sects[j].buffer + ofs) += sects[SEC_TEXT].size + sects[SEC_DATA].size;
                        switch (j) {
                        case SEC_TEXT:
                            AddCodeBaseFixup(ofs);
                            break;
                        case SEC_DATA:
                            AddCodeBaseFixup(ofs + sects[SEC_TEXT].size);
                            break;
                        case SEC_RDATA:
                            AddCodeBaseFixup(ofs + sects[SEC_TEXT].size + sects[SEC_DATA].size - rdata_ofs);
                            break;
                        case SEC_PTDATA:
                            AddPatchBaseFixup(ofs);
                            break;
                        default:
                            fputs("Fixup referencing invalid section.\n", stderr);
                            return 1;
                        }
                    }
                } else if (value >= sects[SEC_PTDATA].vaddr && value < sects[SEC_PTDATA].vaddr + sects[SEC_PTDATA].size) {
                    if (value >= patch_offset && value < patch_offset + patch_size) {
                        fprintf(stderr, "Fixup record pointing to patch data! This shouldn't happen!\n");
                        return 1;
                    }
                    fprintf(stderr, "Fixup record pointing to `.ptdata' section, but not to patch data!!!\n");
                    return 1;
                } else {
                    /* great troubles here; in MS VC++'s output() function, a
                       table is accessed with 32 bytes negative displacement
                       as a result of optimizations; we'll give this fixup to
                       next section to compensate for this behaviour
                    */
                    for (min_delta = UINT_MAX, k = 0; k < sects_needed; k++) {
                        if (sects[k].vaddr > value && sects[k].vaddr <
                            min_delta) {
                            next_sec = k;
                            min_delta = sects[k].vaddr;
                        }
                    }
                    /* I'll cover only SEC_RDATA case because I know it's the only one for now */
                    if (next_sec == SEC_RDATA) {
                        *(dword *)(sects[j].buffer + ofs) -= sects[SEC_RDATA].vaddr;
                        AddCodeBaseFixup(ofs + sects[SEC_TEXT].size + sects[SEC_DATA].size - rdata_ofs);
                    }
                    fprintf(stderr, "Fixup record pointing to unknown section (from section `%s').\n", sects[j].name);
                    fprintf(stderr, "value: 0x%08x offset: 0x%08x type = %d ordinal %d, total %d\n", value, ofs, type, i, num_relocs);
                    /* don't end program here; we issued a warning, image probably won't run, but maybe this is some kind of experiment */
                    warnings++;
                }
                break;
            default:
                fprintf(stderr, "Unexpected fixup type: %d\n", type);
                return 1;
            }
        }
    }

    /* close input file */
    fclose(in);

    /* use len as a relocations sections delimiter */
    len = -1;

    /* calculate checksum */
    c = Checksum(0, size = 0, sects[SEC_TEXT].buffer, sects[SEC_TEXT].size);
    c = Checksum(c, size += sects[SEC_TEXT].size, sects[SEC_DATA].buffer, sects[SEC_DATA].size);
    c = Checksum(c, size += sects[SEC_DATA].size, sects[SEC_RDATA].buffer + rdata_ofs, rdata_size);
    c = Checksum(c, size += rdata_size, sects[SEC_PTDATA].buffer + patch_offset, patch_size);
    c = Checksum(c, size += patch_size, (char *)code_swosc_fixup, csc_ptr * sizeof(dword));
    c = Checksum(c, size += csc_ptr * sizeof(dword), (char *)&len, sizeof(dword));
    c = Checksum(c, size += sizeof(dword), (char *)code_swosd_fixup, csd_ptr * sizeof(dword));
    c = Checksum(c, size += csd_ptr * sizeof(dword), (char *)&len, sizeof(dword));
    c = Checksum(c, size += sizeof(dword), (char *)code_base_fixup, cb_ptr * sizeof(dword));
    c = Checksum(c, size += cb_ptr * sizeof(dword),(char *)&len, sizeof(dword));
    c = Checksum(c, size += sizeof(dword), (char *)patch_swosc_fixup, psc_ptr * sizeof(dword));
    c = Checksum(c, size += psc_ptr * sizeof(dword), (char *)&len,sizeof(dword));
    c = Checksum(c, size += sizeof(dword), (char *)patch_swosd_fixup, psd_ptr * sizeof(dword));
    c = Checksum(c, size += psd_ptr * sizeof(dword), (char *)&len,sizeof(dword));
    c = Checksum(c, size += sizeof(dword), (char *)patch_base_fixup, pb_ptr * sizeof(dword));
    c = Checksum(c, size + pb_ptr * sizeof(dword), (char *)&len, sizeof(dword));

    /* do it portably :P */
    zk.signature[0] = 'Z';
    zk.signature[1] = 'K';
    zk.signature[2] = 'B';
    zk.signature[3] = 'F';
    zk.ver_major = 1;
    zk.ver_minor = 0;
    zk.header_size = sizeof(ZK_header);
    zk.checksum = c;
    zk.code_ofs = sizeof(ZK_header);
    zk.code_size = sects[SEC_TEXT].size + sects[SEC_DATA].size + rdata_size;
    code_data_pad = (4 - (zk.code_size & 3)) & 3;
    zk.patch_offset = zk.code_ofs + zk.code_size + code_data_pad;
    patch_pad = (4 - (patch_size & 3)) & 3;
    zk.patch_size = patch_size;
    zk.reloc_ofs = zk.patch_offset + zk.patch_size + patch_pad;
    zk.reloc_size = 4 * csc_ptr + 4 * csd_ptr + 4 * cb_ptr + 4 * psc_ptr + 4 * psd_ptr + 4 * pb_ptr + 24;
    zk.bss_size = bss_size;
    zk.entry_point = peopt.entry_point - sects[SEC_TEXT].vaddr;

    if (!(out = fopen(out_file, "wb"))) {
        fprintf(stderr, "Can't open %s for writing\n", out_file);
        return 1;
    }

    /* write out bin file */
    safefwrite(&zk, sizeof(ZK_header), 1, out);
    safefwrite(sects[SEC_TEXT].buffer, sects[SEC_TEXT].size, 1, out);
    safefwrite(sects[SEC_DATA].buffer, sects[SEC_DATA].size, 1, out);
    safefwrite(sects[SEC_RDATA].buffer + rdata_ofs, rdata_size, 1, out);
    safefwrite("\0\0", code_data_pad, 1, out);
    safefwrite(sects[SEC_PTDATA].buffer + patch_offset, patch_size, 1, out);
    safefwrite("\0\0", patch_pad, 1, out);
    safefwrite(code_swosc_fixup, csc_ptr * sizeof(dword), 1, out);
    safefwrite(&len, sizeof(dword), 1, out);
    safefwrite(code_swosd_fixup, csd_ptr * sizeof(dword), 1 ,out);
    safefwrite(&len, sizeof(dword), 1, out);
    safefwrite(code_base_fixup, cb_ptr * sizeof(dword), 1, out);
    safefwrite(&len, sizeof(dword), 1, out);
    safefwrite(patch_swosc_fixup, psc_ptr * sizeof(dword), 1, out);
    safefwrite(&len, sizeof(dword), 1, out);
    safefwrite(patch_swosd_fixup, psd_ptr * sizeof(dword), 1, out);
    safefwrite(&len, sizeof(dword), 1, out);
    safefwrite(patch_base_fixup, pb_ptr * sizeof(dword), 1, out);
    safefwrite(&len, sizeof(dword), 1, out);

    /* free sections */
    for (i = 0; i < sects_needed; i++) {
        if (sects[i].buffer) {
            free(sects[i].buffer);
            sects[i].buffer = NULL;
        }
    }
    fclose(out);
    if (warnings)
        fprintf(stderr, "%d warning(s). Image may not run.\n", warnings);
    else
        fputs("All OK.\n", stderr);
    fprintf(stderr, "Memory required for loading: %d bytes\n", zk.code_size + zk.bss_size + zk.patch_size + zk.reloc_size);

    return 0;
}