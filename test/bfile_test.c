#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "types.h"
#define rand __rand
#define strtol __strtol
#include "util.h"
#undef rand
#include "dos.h"
#include "qalloc.h"
#include "bfile_mock.h"

/* can't include Windows.h or all hell will break loose */
extern unsigned long __cdecl _exception_code(void);
extern void * __cdecl _exception_info(void);

static char original[200 * 1024];
static char copy[sizeof(original)];
static const char testFileName[] = "test.bin";

static void XWriteBFile(BFile *file, char **data, int *remaining, int size)
{
    int bytesWritten = WriteBFile(file, *data, size), i;
    for (i = 1; i < file->bytesInBuffer; i++)
        assert(file->buffer[i]  == (char)(file->buffer[i - 1] + 1));
    if (bytesWritten < 0) {
        printf("Error writing data (*data = %#x, size = %d).\n", *data, size);
        _asm int 3
        exit(1);
    }
    if (bytesWritten != size) {
        printf("Partial write: size = %d, written = %d.\n", size, bytesWritten);
        _asm int 3
        exit(1);
    }
    *data += size;
    *remaining -= size;
    assert(*remaining >= 0);
    assert(size >= 0);
}

static void VerifyRead(int *remaining, int size)
{
    int i;
    for (i = 0; i < sizeof(copy) - *remaining; i++)
        assert(copy[i] == (char)i % 256);
    assert(*remaining >= 0);
    assert(size >= 0);
}

static void XReadBFile(BFile *file, char **data, int *remaining, int size)
{
    int bytesRead = ReadBFile(file, *data, size);
    if (bytesRead < 0) {
        printf("Error reading data (*data = %#x, size = %d).\n", *data, size);
        _asm int 3
        exit(1);
    }
    if (bytesRead != size) {
        printf("Partial read: size = %d, read = %d.\n", size, bytesRead);
        _asm int 3
        exit(1);
    }
    *data += size;
    *remaining -= size;
    VerifyRead(remaining, size);
}

static void XReadByteByByte(BFile *file, char **data, int *remaining, int size)
{
    int i, c;
    for (i = 0; i < size; i++) {
        c = PeekCharBFile(file);
        assert(c == GetCharBFile(file));
        UngetCharBFile(file, c);
        c = PeekCharBFile(file);
        assert(c == GetCharBFile(file));
        *(*data)++ = c;
    }
    *remaining -= size;
    VerifyRead(remaining, size);
}

static int DoTest()
{
    BFile *file;
    char *data = original;
    int size = sizeof(original), fileOffset;
    int i, c;

    file = CreateBFile(ATTR_NONE, testFileName);
    if (!file) {
        printf("Failed to create binary test file %s.\n", testFileName);
        return 1;
    }
    /* write entire buffer */
    XWriteBFile(file, &data, &size, FILE_BUFFER_SIZE);
    /* few buffered writes */
    XWriteBFile(file, &data, &size, 20);
    XWriteBFile(file, &data, &size, 150);
    XWriteBFile(file, &data, &size, 1);
    /* zero bytes write */
    XWriteBFile(file, &data, &size, 0);
    /* now mixed write, buffer + direct */
    XWriteBFile(file, &data, &size, FILE_BUFFER_SIZE);
    /* few small writes again */
    XWriteBFile(file, &data, &size, 147);
    XWriteBFile(file, &data, &size, 29);
    XWriteBFile(file, &data, &size, 13);
    /* followed by a big write */
    XWriteBFile(file, &data, &size, 3 * FILE_BUFFER_SIZE + 333);
    /* save the rest randomly */
    while (size) {
        int blockSize = rand() % (2 * FILE_BUFFER_SIZE);
        blockSize = min(blockSize, size);
        XWriteBFile(file, &data, &size, blockSize);
    }
    FlushBFile(file);
    fileOffset = SeekBFile(file, SEEK_CUR, 0, 0);
    assert(fileOffset == sizeof(original));
    CloseBFile(file);
    assert(size == 0);
    assert(data == original + sizeof(original));

    /* now read test */
    file = OpenBFile(F_READ_ONLY, testFileName);
    if (!file) {
        printf("Failed to open binary test file %s.\n", testFileName);
        return 1;
    }
    data = copy;
    size = sizeof(copy);
    /* read size of buffer to warm up */
    XReadBFile(file, &data, &size, FILE_BUFFER_SIZE);
    /* followed by a few small reads */
    c = PeekCharBFile(file);
    XReadBFile(file, &data, &size, 1);
    assert(c == data[0]);
    UngetCharBFile(file, c);
    c = PeekCharBFile(file);
    data[0] = GetCharBFile(file);
    assert(c == data[0]);
    XReadBFile(file, &data, &size, 27);
    XReadBFile(file, &data, &size, 1);
    XReadBFile(file, &data, &size, 19);
    /* zero bytes read */
    XReadBFile(file, &data, &size, 0);
    /* cross buffer boundary read */
    XReadBFile(file, &data, &size, FILE_BUFFER_SIZE);
    /* big read */
    XReadBFile(file, &data, &size, 3 * FILE_BUFFER_SIZE + 347);
    /* few small reads again */
    XReadBFile(file, &data, &size, 42);
    XReadBFile(file, &data, &size, 2391);
    XReadBFile(file, &data, &size, 17);
    /* read the rest in randomly */
    while (size) {
        int blockSize = rand() % (2 * FILE_BUFFER_SIZE);
        blockSize = min(blockSize, size);
        if (rand() & 1)
            XReadByteByByte(file, &data, &size, blockSize);
        else
            XReadBFile(file, &data, &size, blockSize);
    }
    CloseBFile(file);
    assert(size == 0);

    /* and finally, test if data is a match */
    for (i = 1; i < sizeof(copy); i++)
        assert(copy[i] == (char)(copy[i - 1] + 1));
    if (memcmp(original, copy, sizeof(original))) {
        BFile *orig;
        printf("Data mismatch. Data that was saved and read back differs from the original!\n");
        orig = CreateBFile(ATTR_NONE, "orig.bin");
        if (orig) {
            WriteBFile(orig, copy, sizeof(copy));
            CloseBFile(orig);
        }
        return 1;
    }

    /* passed! */
    return 0;
}

int main()
{
    int i;
    extern void TurnLogOn();

    TurnLogOn();
    qAllocInit();
    atexit((void (*)())getchar);
    atexit(qAllocFinish);
    srand(time(nullptr));
    /* this will come in great for verification - each byte is greater than the last */
    for (i = 0; i < sizeof(original); i++)
        original[i] = i;

    SetupWinFileEmulationLayer((uint8_t *)OpenBFile, (uint8_t *)OpenBFile + 3 * 1024);
    __try {
        return DoTest();
    } __except (HandleException(_exception_code(), _exception_info())) {}

    return 1;
}