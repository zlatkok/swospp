/**
    Simple, quick memory allocator - returns first found block that satisfies requirements.
    Debug version will offer some error-checking facilities and track memory usage.
    Meant to be used for small allocations - for heaps up to 64kb.
*/

#include "qalloc.h"

#define INTERNAL_HEAP_SIZE  45 * 1024   /* should be enough for all packets at once */
static char internalHeap[INTERNAL_HEAP_SIZE];
static int minimumSplitSize = 128;

/* only works with heaps up to 64kb size, hi-hi */
struct Block {
    word next;
    word prev;
    word size;
#ifdef DEBUG
    char origin[16];    /* track allocation origin when reporting memory leaks */
    word sig;           /* try to capture memory overwrites */
#endif
};

#ifdef DEBUG
#define setSig(p)   (((Block *)(p))->sig = 0xbdfd)
#define checkSig(p) (assert(((Block *)(p))->sig == 0xbdfd))

/* per heap statistics */
struct HeapStats {
    word currentAllocations;
    word currentlyAllocated;
    word maxAllocations;
    word maxAllocated;
    word excludedBlocks[16];    /* don't report these blocks as memory leaks */
    word numExcludedBlocks;
    void *heap;
};

static HeapStats heapStats[16];
static int numHeaps;

static HeapStats *GetHeapStats(void *heap)
{
    int i;
    assert(numHeaps >= 0 && numHeaps < (int)sizeofarray(heapStats));
    for (i = 0; i < numHeaps; i++)
        if (heapStats[i].heap == heap)
            return &heapStats[i];
    return nullptr;
}

static void InitHeapStats(void *heap)
{
    HeapStats *hs;
    assert(numHeaps >= 0 && numHeaps < (int)sizeofarray(heapStats));
    if (!(hs = GetHeapStats(heap)))
        hs = &heapStats[numHeaps++];
    hs->currentAllocations = hs->currentlyAllocated = hs->maxAllocations = hs->maxAllocated = 0;
    hs->heap = heap;
}

static void RemoveHeapStats(void *heap)
{
    int i;
    assert(numHeaps >= 0 && numHeaps < (int)sizeofarray(heapStats) && heap);
    for (i = 0; i < numHeaps; i++) {
        if (heapStats[i].heap == heap) {
            memmove(heapStats + i, heapStats + i + 1, numHeaps - i - 1);
            numHeaps--;
            return;
        }
    }

    WriteToLog("qAlloc: Tried to remove non-existent heap stats.");
}

/** ExcludeHeapBlocks

    heap -> heap we're working with
    size -  size of the heap

    Go through heap and mark each currently allocated block as excluded from memory leak checking.
    Useful in case you have some blocks that will stay allocated throughout the whole program life cycle
    without getting explicitly freed, causing final memory check to report them as memory leaks.
*/
void ExcludeHeapBlocks(void *heap, int size)
{
    Block *head = (Block *)heap, *p = head;
    HeapStats *hs = GetHeapStats(heap);
    assert(hs);

    do {
        checkSig(p);
        if (!p->size && p != (Block *)((char *)heap + size - sizeof(Block)))
            hs->excludedBlocks[hs->numExcludedBlocks++] = p - head;
        p = (Block *)((char *)heap + p->next);
    } while (p != head);
}

bool IsBlockExcluded(const HeapStats *hs, word ofs)
{
    assert(hs);
    for (size_t i = 0; i < hs->numExcludedBlocks; i++)
        if (hs->excludedBlocks[i] == ofs)
            return true;

    return false;
}

void ExcludeBlocks()
{
    ExcludeHeapBlocks(internalHeap, sizeof(internalHeap));
}

/** ValidateHeap

    heap -> heap to validate

    Needed for tests, but you can call it if you suspect heap corruption at some point. Compiled only in
    debug build, will assert if any irregularity is found.
*/
void ValidateHeap(void *heap)
{
    Block *head = (Block *)heap, *p = (Block *)((char *)heap + head->next), *prev = head, *next = head;
    checkSig(head);
    while (p != head) {
        checkSig(p);
        if (prev->size) {
            assert((dword)prev + prev->size == (dword)p);
            assert(!p->size);
        }
        prev = p;
        p = (Block *)((char *)heap + p->next);
    }

    p = (Block *)((char *)heap + head->prev);
    prev = (Block *)((char *)heap + p->prev);

    while (p != head) {
        checkSig(p);
        if (next->size && next != heap)
            assert(!p->size);
        if (prev->size)
            assert((dword)prev + prev->size == (dword)p);
        next = p;
        p = prev;
        prev = (Block *)((char *)heap + p->prev);
    }

    if (head->prev == head->next && !head->prev)
        assert(head->size);
}

/** CheckMemoryLeaks

    heap -> heap to test
    size -  size of given heap

    Check this heap for memory leaks - left over memory blocks, and issue a report about any found:
    write size and source file location which performed allocation.
*/
void CheckMemoryLeaks(void *heap, int size)
{
    HeapStats *hs = GetHeapStats(heap);
    /* check for memory leaks */
    bool leaks = false;
    int numExcludedBlocks = 0;
    int totalExcludedMem = 0;
    Block *head = (Block *)heap, *p = head;
    assert(hs);

    do {
        checkSig(p);
        if (!p->size && p != (Block *)((char *)heap + size - sizeof(Block))) {
            int blockSize = p->next - ((dword)p - (dword)heap) - sizeof(Block);

            if (!IsBlockExcluded(hs, (char *)p - (char *)heap)) {
                WriteToLog("qAlloc: Left over memory block at %#x, size %d, allocated at %s.\n",
                    (dword)p + sizeof(Block), blockSize, p->origin);
                HexDumpToLog((char *)p + sizeof(Block), p->next - ((dword)p - (dword)internalHeap) - sizeof(Block),
                    "left over memory block");
                leaks = true;
            } else {
                numExcludedBlocks++;
                totalExcludedMem += blockSize + sizeof(Block);
            }
        }
        p = (Block *)((char *)heap + p->next);
    } while (p != head);

    WriteToLog("qAlloc: Maximum memory allocated: %d bytes, maximum simultaneous allocations: %d",
        hs->maxAllocated, hs->maxAllocations);

    if (!leaks) {
        assert(!(hs->currentAllocations - numExcludedBlocks) && !(hs->currentlyAllocated - totalExcludedMem));
        WriteToLog("qAlloc OK. No memory leaks.");
    }

    RemoveHeapStats(heap);
}

/* Important for edge case testing. */
int GetMaxAllocSize(int heapSize)
{
    return heapSize - 2 * sizeof(Block);
}

#else
#define setSig(p)       ((void)0)
#define checkSig(p)     ((void)0)
#endif

static void AddBlock(char *heap, Block *oldBlock, int size)
{
    assert(oldBlock->size >= size + sizeof(Block));
    /* don't split blocks too small */
    if (size >= (int)oldBlock->size - (int)sizeof(Block) - minimumSplitSize)
        oldBlock->size = 0;
    else {
        Block *newBlock = (Block *)((char *)oldBlock + size + sizeof(Block));
        ((Block *)(heap + oldBlock->next))->prev = (char *)newBlock - heap;
        newBlock->next = oldBlock->next;
        newBlock->prev = (char *)oldBlock - heap;
        oldBlock->next = (char *)newBlock - heap;
        newBlock->size = oldBlock->size - size - sizeof(Block);
        oldBlock->size = 0;
        setSig(newBlock);
    }
}

/* Detach block off of linked list. */
static void removeBlock(char *heap, Block *p)
{
    checkSig(p);
    ((Block *)(heap + p->prev))->next = p->next;
    ((Block *)(heap + p->next))->prev = p->prev;
}

/* This will be taken care of by initializing code. */
void qAllocInit()
{
    qHeapInit(internalHeap, sizeof(internalHeap));
#ifdef DEBUG
    InitHeapStats(internalHeap);
#endif
}

void qAllocFinish()
{
#ifdef DEBUG
    CheckMemoryLeaks(internalHeap, sizeof(internalHeap));
#endif
}

void qHeapInit(void *heap, int size)
{
    Block *first = (Block *)heap, *last = (Block *)((char *)heap + size - sizeof(Block));
    assert(size > 2 * (int)sizeof(Block));
    first->next = first->prev = (dword)last - (dword)heap;
    last->next = last->prev = 0;
    first->size = size - sizeof(Block);
    last->size = 0;
    setSig(first);
    setSig(last);
#ifdef DEBUG
    InitHeapStats(heap);
#endif
}

#ifdef DEBUG
void *qHeapAlloc_(void *heap, int size, const char *file, int line)
#else
void *qHeapAlloc(void *heap, int size)
#endif
{
    /* traverse through blocks list and return 1st block that fits */
    Block *head = (Block *)heap, *p = head;
    do {
        checkSig(p);
        if (p->size >= size + sizeof(Block)) {
            AddBlock((char *)heap, p, size);
#ifdef DEBUG
            {
                HeapStats *hs = GetHeapStats(heap);
                const char *origin = (const char *)strrchr((char *)file, '\\');
                origin = origin ? origin + 1 : file;
                sprintf(p->origin, "%.10s:%d", origin, line > 9999 ? 9999 : line);
                assert(hs);
                hs->currentAllocations++;
                hs->maxAllocations = max(hs->maxAllocations, hs->currentAllocations);
                hs->currentlyAllocated += p->next - ((dword)p - (dword)heap);
                hs->maxAllocated = max(hs->maxAllocated, hs->currentlyAllocated);
            }
#endif
            return (char *)p + sizeof(Block);
        }
        p = (Block *)((char *)heap + p->next);
    } while (p != head);

    WriteToLog("qAlloc: Out of memory! Requested: %d bytes (heap is %#x)", size, heap);
    return nullptr;
}

void qHeapFree(void *heap, void *ptr)
{
    if (!ptr)
        return;

    /* restore block's size, try to merge it with neighbours */
    Block *p = (Block *)((char *)ptr - sizeof(Block)), *prev, *next;
    checkSig(p);
    assert(!p->size);
    p->size = p->next - ((dword)p - (dword)heap);
#ifdef DEBUG
    {
        HeapStats *hs = GetHeapStats(heap);
        assert(hs);
        hs->currentAllocations--;
        hs->currentlyAllocated -= p->size;
    }
#endif
    prev = ((Block *)((char *)heap + p->prev));
    checkSig(prev);
    if (p != heap && prev->size) {
        prev->size += p->size;
        removeBlock((char *)heap, p);
        p = prev;
    }

    next = ((Block *)((char *)heap + p->next));
    if (next != heap && next->size) {
        p->size += next->size;
        removeBlock((char *)heap, next);
    }

#ifdef DEBUG
    memset((char *)p + sizeof(Block), 0xee, p->size - sizeof(Block));
#endif
}

#ifdef DEBUG
void *qAlloc_(int size, const char *file, int line)
{
    return qHeapAlloc_(internalHeap, size, file, line);
}
#else
void *qAlloc(int size)
{
    return qHeapAlloc(internalHeap, size);
}
#endif

void qFree(void *ptr)
{
    assert(!ptr || (ptr > internalHeap && ptr < internalHeap + INTERNAL_HEAP_SIZE));
    qHeapFree(internalHeap, ptr);
}

/** qSetMinSplitSize

    newSplitSize - new minimal size below which isn't feasible to split free blocks

    Returns old split size.
*/
int qSetMinSplitSize(int newSplitSize)
{
    int oldSize = minimumSplitSize;
    newSplitSize = max(16, newSplitSize);
    newSplitSize = min(1024, newSplitSize);
    minimumSplitSize = newSplitSize;
    return oldSize;
}
