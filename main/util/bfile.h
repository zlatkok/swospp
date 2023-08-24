#pragma once

/* buffered file routines */
struct BFile {
    char *buffer;
    int bufferSize;
    int bytesInBuffer;
    int readPtr;
    dword handle;
    bool managed;
};

#define FILE_BUFFER_SIZE (4 * 1024)

extern "C" {
    BFile *OpenBFile(DOS_accessMode accessMode, const char *filename);
    bool OpenBFileUnmanaged(BFile *file, void *buffer, int bufferSize, DOS_accessMode accessMode, const char *filename);
    BFile *CreateBFile(DOS_fileAttributes fileAttribute, const char *filename);
    bool CreateBFileUnmanaged(BFile *file, void *buffer, int bufferSize, DOS_fileAttributes fileAttribute, const char *filename);
    int FlushBFile(BFile *file);
    int WriteBFile(BFile *file, const void *pData, int size);
    bool PutCharBFile(BFile *file, char c);
    int ReadBFile(BFile *file, void *pData, int size);
    int GetCharBFile(BFile *file);
    int PeekCharBFile(BFile *file);
    bool UngetCharBFile(BFile *file, int c);
    int SeekBFile(BFile *file, uchar mode, int offset);
    int GetBFilePos(BFile *file);
    void CloseBFile(BFile *file);
    void CloseBFileUnmanaged(BFile *file);
}
