/**
    Buffered File support routines, built on top of DOS direct file access functions.
**/

#include "bfile.h"
#include "qalloc.h"

static BFile *initBFile(BFile *file, void *buffer, int bufferSize, bool readOnly, bool managed)
{
    assert(file);

    file->bytesInBuffer = 0;
    file->readPtr = readOnly ? 0 : -1;
    file->buffer = (char *)buffer;
    file->bufferSize = bufferSize;
    file->managed = managed;

    if (file->handle == INVALID_HANDLE) {
        if (managed)
            qFree(file);
        return nullptr;
    }

    if (!managed)
        file->bufferSize = bufferSize;
    else if (!(file->buffer = (char *)qAlloc(FILE_BUFFER_SIZE))) {
        CloseFile(file->handle);
        qFree(file);
        return nullptr;
    }

    return file;
}

BFile *OpenBFile(DOS_accessMode accessMode, const char *filename)
{
    assert_msg(accessMode != F_READ_WRITE, "BFile can't be open for both read and write.");
    auto file = (BFile *)qAlloc(sizeof(BFile));

    if (!file)
        return nullptr;

    assert(filename);
    file->handle = (dword)OpenFile(accessMode, filename);
    return initBFile(file, nullptr, FILE_BUFFER_SIZE, accessMode == F_READ_ONLY, true);
}

bool OpenBFileUnmanaged(BFile *file, void *buffer, int bufferSize, DOS_accessMode accessMode, const char *filename)
{
    assert(file);
    assert_msg(accessMode != F_READ_WRITE, "BFile can't be open for both read and write.");
    file->handle = OpenFile(accessMode, filename);
    return initBFile(file, buffer, bufferSize, accessMode == F_READ_ONLY, false) != nullptr;
}

BFile *CreateBFile(DOS_fileAttributes fileAttribute, const char *filename)
{
    auto file = (BFile *)qAlloc(sizeof(BFile));

    if (!file)
        return nullptr;

    assert(filename);
    file->handle = CreateFile(fileAttribute, filename);
    return initBFile(file, nullptr, FILE_BUFFER_SIZE, false, true);
}

bool CreateBFileUnmanaged(BFile *file, void *buffer, int bufferSize, DOS_fileAttributes fileAttribute, const char *filename)
{
    assert(file);
    file->handle = CreateFile(fileAttribute, filename);
    return initBFile(file, buffer, bufferSize, false, false) != nullptr;
}

int FlushBFile(BFile *file)
{
    assert(file);
    /* a NOP for read-only files */
    if (file->readPtr >= 0)
        return 0;

    assert(file->bytesInBuffer >= 0 && file->bytesInBuffer <= file->bufferSize);
    if (file->bytesInBuffer) {
        int bytesWritten = WriteFile(file->handle, file->buffer, file->bytesInBuffer);
        if (bytesWritten < 0)
            return bytesWritten;

        assert(bytesWritten <= file->bytesInBuffer);

        /* partial write */
        if (bytesWritten != file->bytesInBuffer)
            memmove(file->buffer, file->buffer + bytesWritten, file->bytesInBuffer - bytesWritten);

        file->bytesInBuffer -= bytesWritten;
        return bytesWritten;
    }

    return 0;
}

int WriteBFile(BFile *file, const void *pData, int size)
{
    assert(size >= 0 && pData && file);
    assert(file->readPtr < 0);

    if (!size)
        return 0;

    if (size > file->bufferSize) {
        /* for big writes just do it directly, but first flush whatever we got in the buffer */
        int bytesInBuffer = file->bytesInBuffer;
        int bytesWritten = FlushBFile(file);

        if (bytesWritten < 0)
            return bytesWritten;

        if (bytesWritten != bytesInBuffer)
            return 0;

        return WriteFile(file->handle, pData, size);
    } else {
        int remainingBufSize = file->bufferSize - file->bytesInBuffer;
        if (size <= remainingBufSize) {
            /* entirely buffered */
            memcpy(file->buffer + file->bytesInBuffer, pData, size);
            file->bytesInBuffer += size;
            assert(file->bytesInBuffer <= file->bufferSize);
            return size;
        } else {
            /* buffer flush, start over */
            int bytesWritten;
            memcpy(file->buffer + file->bytesInBuffer, pData, remainingBufSize);
            assert(file->bytesInBuffer + remainingBufSize == file->bufferSize);
            file->bytesInBuffer = file->bufferSize;
            bytesWritten = FlushBFile(file);

            if (bytesWritten < 0)
                return bytesWritten;

            if (file->bytesInBuffer)
                return 0;

            memcpy(file->buffer, (char *)pData + remainingBufSize, size - remainingBufSize);
            file->bytesInBuffer = size - remainingBufSize;
            assert(file->bytesInBuffer <= file->bufferSize);

            return size;
        }
    }
}

bool PutCharBFile(BFile *file, char c)
{
    return WriteBFile(file, &c, 1) == 1;
}

int ReadBFile(BFile *file, void *pData, int size)
{
    int bytesRead, originalSize = size;
    auto data = (char *)pData;
    assert(file && pData && size >= 0);
    assert(file->readPtr >= 0);

    if (!size)
        return 0;

    /* get anything we have in buffer first */
    if (file->readPtr < file->bytesInBuffer) {
        int bytesInBuffer = file->bytesInBuffer - file->readPtr;
        if (size > bytesInBuffer) {
            memcpy(data, file->buffer + file->readPtr, bytesInBuffer);
            size -= bytesInBuffer;
            data += bytesInBuffer;

            if (size > file->bufferSize) {
                file->bytesInBuffer = 0;
                return ReadFile(file->handle, data, size) + bytesInBuffer;
            }
        } else {
            /* entirely in buffer */
            memcpy(data, file->buffer + file->readPtr, size);
            file->readPtr += size;
            return size;
        }
    } else {
        /* buffer empty, reset it */
        file->readPtr = 0;
        file->bytesInBuffer = 0;

        if (size > file->bufferSize)
            return ReadFile(file->handle, data, size);
    }
    /* fill buffer and return data */
    bytesRead = ReadFile(file->handle, file->buffer, file->bufferSize);
    if (bytesRead < 0)
        return bytesRead;

    file->readPtr = min(size, bytesRead);
    file->bytesInBuffer = bytesRead;
    memcpy(data, file->buffer, file->readPtr);
    return file->readPtr + originalSize - size;
}

int GetCharBFile(BFile *file)
{
    int c = 0, bytesRead = ReadBFile(file, &c, 1);
    if (bytesRead != 1)
        return -1;

    return c;
}

int PeekCharBFile(BFile *file)
{
    assert(file && file->readPtr >= 0);
    if (file->readPtr < file->bytesInBuffer)
        return file->buffer[file->readPtr] & 0xff;
    else {
        /* refill the buffer */
        int bytesRead = ReadFile(file->handle, file->buffer, file->bufferSize);
        if (bytesRead < 0)
            return -1;

        file->bytesInBuffer = bytesRead;
        file->readPtr = 0;

        if (!bytesRead)
            return -1;

        return file->buffer[0] & 0xff;
    }
}

bool UngetCharBFile(BFile *file, int c)
{
    assert(file && file->readPtr >= 0);
    assert(file->readPtr <= file->bufferSize);
    assert(file->bufferSize >= 0);
    if (c < 0)
        return true;

    if (file->bytesInBuffer > 0) {
        assert(file->bufferSize >= file->readPtr);
        if (file->readPtr > 0)
            file->buffer[--file->readPtr] = c;
        else {
            bool seekBack = file->bytesInBuffer == file->bufferSize;
            memmove(file->buffer + 1, file->buffer, min(file->bytesInBuffer, file->bufferSize - 1));
            file->bytesInBuffer = min(file->bytesInBuffer + 1, file->bufferSize);
            file->buffer[0] = c;

            /* seek back 1 byte in file, otherwise in the next read we will miss that byte that was kicked out */
            if (seekBack)
                return SeekFile(file->handle, SEEK_CUR, -1, -1) != -1;
        }
    } else {
        file->buffer[0] = c;
        file->bytesInBuffer = 1;
        file->readPtr = 0;
    }
    return true;
}

int SeekBFile(BFile *file, uchar mode, int offset)
{
    // if the seek offset falls within our read buffer just adjust readPtr
    if (file->readPtr >= 0) {
        int currentPos = GetFilePos(file->handle);
        switch (mode) {
        case SEEK_START:
            if (offset >= currentPos - file->bytesInBuffer && offset < currentPos) {
                file->readPtr = offset - (currentPos - file->bytesInBuffer);
                return offset;
            }
            break;
        case SEEK_CUR:
            if (offset >= 0) {
                if (file->readPtr + offset < file->bytesInBuffer)
                    file->readPtr += offset;
                return currentPos - file->bytesInBuffer + file->readPtr;
            } else if (offset >= -file->readPtr) {
                file->readPtr += offset;
                return currentPos - file->bytesInBuffer + file->readPtr;
            }
            break;
        }
        // discard the buffer and seek
        file->readPtr = 0;
        file->bytesInBuffer = 0;
    } else {
        FlushBFile(file);
    }

    return SeekFile(file->handle, mode, offset >> 16, offset);
}

int GetBFilePos(BFile *file)
{
    if (file->readPtr >= 0)
        return GetFilePos(file->handle) - file->bytesInBuffer + file->readPtr;
    else
        return GetFilePos(file->handle) + file->bytesInBuffer;
}

void CloseBFile(BFile *file)
{
    assert(file && file->buffer);
    FlushBFile(file);
    CloseFile(file->handle);
    qFree(file->buffer);
    qFree(file);
}

void CloseBFileUnmanaged(BFile *file)
{
    assert(file);
    FlushBFile(file);
    CloseFile(file->handle);
}
