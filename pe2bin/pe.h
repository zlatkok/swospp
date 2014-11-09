#ifndef PE_H
#define PE_H

/* v1.3 */

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;
typedef unsigned char uchar;
typedef unsigned int uint;

/* turn on structure packing */
#if defined __WATCOMC__ || _MSC_VER
#pragma pack(push, 1)
#elif defined __GNUC__
#pragma align 1
#else
#error "Please define structure packing directive for your compiler"
#endif

#define PE_SIGNATURE 0x00004550

/* directory entries */
enum dir_entries {
    DIR_EXPORT,
    DIR_IMPORT,
    DIR_RESOURCE,
    DIR_EXCEPTION,
    DIR_SECURITY,
    DIR_BASERELOC,
    DIR_DEBUG,
    DIR_COPYRIGHT,
    DIR_GLOBALPTR,
    DIR_TLS,
    DIR_LOAD_CONFIG,
    DIR_BOUND_IMPORT,
    DIR_IAT,
    DIR_DELAY_LOAD,
    DIR_COM,
    DIR_RESERVED
};

/* section flags */
enum section_flags {
    SCN_TYPE_REG               = 0x00000000,
    SCN_TYPE_DSECT             = 0x00000001,
    SCN_TYPE_NOLOAD            = 0x00000002,
    SCN_TYPE_GROUP             = 0x00000004,
    SCN_TYPE_NO_PAD            = 0x00000008,
    SCN_TYPE_COPY              = 0x00000010,
    SCN_CNT_CODE               = 0x00000020,
    SCN_CNT_INITIALIZED_DATA   = 0x00000040,
    SCN_CNT_UNINITIALIZED_DATA = 0x00000080,
    SCN_LNK_OTHER              = 0x00000100,
    SCN_LNK_INFO               = 0x00000200,
    SCN_TYPE_OVER              = 0x00000400,
    SCN_LNK_REMOVE             = 0x00000800,
    SCN_LNK_COMDAT             = 0x00001000,
    SCN_MEM_FARDATA            = 0x00008000,
    SCN_MEM_PURGEABLE          = 0x00010000,
    SCN_MEM_16BIT              = 0x00020000,
    SCN_MEM_LOCKED             = 0x00040000,
    SCN_MEM_PRELOAD            = 0x00080000,
    SCN_ALIGN_1BYTES           = 0x00100000,
    SCN_ALIGN_2BYTES           = 0x00200000,
    SCN_ALIGN_4BYTES           = 0x00300000,
    SCN_ALIGN_8BYTES           = 0x00400000,
    SCN_ALIGN_16BYTES          = 0x00500000,
    SCN_ALIGN_32BYTES          = 0x00600000,
    SCN_ALIGN_64BYTES          = 0x00700000,
    SCN_ALIGN_128BYTES         = 0x00800000,
    SCN_ALIGN_256BYTES         = 0x00900000,
    SCN_ALIGN_512BYTES         = 0x00a00000,
    SCN_ALIGN_1024BYTES        = 0x00b00000,
    SCN_ALIGN_2048BYTES        = 0x00c00000,
    SCN_ALIGN_4096BYTES        = 0x00d00000,
    SCN_ALIGN_8192BYTES        = 0x00e00000,
    SCN_LNK_NRELOC_OVFL        = 0x01000000,
    SCN_MEM_DISCARDABLE        = 0x02000000,
    SCN_MEM_NOT_CACHED         = 0x04000000,
    SCN_MEM_NOT_PAGED          = 0x08000000,
    SCN_MEM_SHARED             = 0x10000000,
    SCN_MEM_EXECUTE            = 0x20000000,
    SCN_MEM_READ               = 0x40000000,
    SCN_MEM_WRITE              = 0x80000000
};

typedef struct _PE_header {
    dword signature;
    word  machine;
    word  num_sections;
    dword time_date_stamp;
    dword sym_table_ptr;
    dword num_symbols;
    word  opt_header_size;
    word  characteristics;
} PE_header;

typedef struct _Data_directory {
    dword vaddr;
    dword size;
} Data_directory;

typedef struct _PE_optional_header {
    word  magic;
    byte  linker_major_ver;
    byte  linker_minor_ver;
    dword sizeof_code;
    dword sizeof_init_data;
    dword sizeof_uninit_data;
    dword entry_point;
    dword code_base;
    dword data_base;
    dword image_base;
    dword section_alignment;
    dword file_alignment;
    word  os_major_ver;
    word  os_minor_ver;
    word  image_major_ver;
    word  image_minor_ver;
    word  subsys_major_ver;
    word  subsys_minor_ver;
    dword reserved1;
    dword sizeof_image;
    dword sizeof_headers;
    dword checksum;
    word  subsystem;
    word  dll_characteristics;
    dword sizeof_stack_reserve;
    dword sizeof_stack_commit;
    dword sizeof_heap_reserve;
    dword sizeof_heap_commit;
    dword loader_flags;
    dword number_of_RVA_and_sizes;
    Data_directory data_dir[16];
} PE_optional_header;

typedef struct _Section_header {
    byte  name[8];
    dword virtual_size;
    dword vaddr;
    dword sizeof_raw_data;
    dword ptr_to_raw_data;
    dword ptr_to_relocations;
    dword ptr_to_line_numbers;
    word  num_relocations;
    word  num_line_numbers;
    dword characteristics;
} Section_header;

typedef struct _Export_directory_table {
    dword export_flags;
    dword time_date_stamp;
    word  major_version;
    word  minor_version;
    dword name_RVA;
    dword ordinal_base;
    dword addr_tbl_entries;
    dword num_name_pointers;
    dword export_addr_tbl_rva;
    dword name_pointer_rva;
    dword ordinal_table_rva;
} Export_directory_table;

typedef struct Import_directory_table {
    dword lookup_table_rva;
    dword time_date_stamp;
    dword forwarder_chain;
    dword name_rva;
    dword iat_rva;
} Import_directory_table;

#if defined __WATCOMC__ || _MSC_VER
#pragma pack(pop)
#elif defined __GNUC__
/* don't know */
#else
#error "Please define structure packing directive for your compiler"
#endif

#endif