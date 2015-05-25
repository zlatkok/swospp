#ifndef PE_H
#define PE_H

/* v1.3 */

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;
typedef unsigned char uchar;
typedef unsigned int uint;

/* turn on structure packing */
#if defined __WATCOMC__ || _MSC_VER || __GNUC__
#pragma pack(push, 1)
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

typedef struct PE_Header {
    dword signature;
    word  machine;
    word  numSections;
    dword timeDateStamp;
    dword symTablePtr;
    dword numSymbols;
    word  optHeaderSize;
    word  characteristics;
} PE_header;

typedef struct DataDirectory {
    dword vaddr;
    dword size;
} DataDirectory;

typedef struct PE_OptionalHeader {
    word  magic;
    byte  linkerMajorVer;
    byte  linkerMinorVer;
    dword sizeOfCode;
    dword sizeOfInitData;
    dword sizeOfUninitData;
    dword entryPoint;
    dword codeBase;
    dword dataBase;
    dword imageBase;
    dword sectionAlignment;
    dword fileAlignment;
    word  osMajorVer;
    word  osMinorVer;
    word  imageMajorVer;
    word  imageMinorVer;
    word  subsysMajorVer;
    word  subsysMinorVer;
    dword reserved1;
    dword sizeOfImage;
    dword sizeOfHeaders;
    dword checksum;
    word  subsystem;
    word  dllCharacteristics;
    dword sizeOfStackReserve;
    dword sizeOfStackCommit;
    dword sizeOfHeapReserve;
    dword sizeOfHeapCommit;
    dword loaderFlags;
    dword numOf_RVA_AndSizes;
    DataDirectory dataDir[16];
} PE_OptionalHeader;

typedef struct SectionHeader {
    byte  name[8];
    dword virtualSize;
    dword vaddr;
    dword sizeOfRawData;
    dword ptrToRawData;
    dword ptrToRelocations;
    dword ptrToLineNumbers;
    word  numRelocations;
    word  numLineNumbers;
    dword characteristics;
} SectionHeader;

typedef struct ExportDirectoryTable {
    dword exportFlags;
    dword timeDateStamp;
    word  majorVersion;
    word  minorVersion;
    dword nameRVA;
    dword ordinalBase;
    dword addrTableEntries;
    dword numNamePointers;
    dword exportAddrTableRVA;
    dword namePointerRVA;
    dword ordinalTableRVA;
} ExportDirectoryTable;

typedef struct ImportDirectoryTable {
    dword lookupTableRVA;
    dword timeDateStamp;
    dword forwarderChain;
    dword nameRVA;
    dword iatRVA;
} ImportDirectoryTable;

#if defined __WATCOMC__ || _MSC_VER || __GNUC__
#pragma pack(pop)
#else
#error "Please define structure packing directive for your compiler"
#endif

#endif