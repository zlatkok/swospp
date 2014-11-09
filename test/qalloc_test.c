#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <crtdbg.h>
#include "qalloc.h"

int main()
{
    const int heapSize = 4 * 1024 + 117, numPtrs = 200, numLoops = 1000000, maxAllocSize = GetMaxAllocSize(heapSize);
    int i, j;
    char *heap = (char *)malloc(heapSize);
    char **ptrs = (char **)malloc(numPtrs * sizeof(char *));
    void *a, *b, *c, *d;
    printf("Starting qAlloc test...\n");
    qHeapInit(heap, heapSize);
    srand((unsigned int)time(NULL));
    a = qHeapAlloc(heap, maxAllocSize); /* maximum allocation */
    assert(a);
    ValidateHeap(heap);
    memset(a, 0x5b, maxAllocSize);
    ValidateHeap(heap);
    qHeapFree(heap, a);
    ValidateHeap(heap);
    a = qHeapAlloc(heap, maxAllocSize - 1);
    ValidateHeap(heap);
    memset(a, 0x5b, maxAllocSize - 1);
    qHeapFree(heap, a);
    ValidateHeap(heap);
    qHeapFree(heap, qHeapAlloc(heap, maxAllocSize - 2));
    ValidateHeap(heap);
    qHeapFree(heap, qHeapAlloc(heap, maxAllocSize - 3));
    ValidateHeap(heap);
    qHeapFree(heap, qHeapAlloc(heap, maxAllocSize - 4));
    ValidateHeap(heap);
    qHeapFree(heap, qHeapAlloc(heap, maxAllocSize - 3));
    ValidateHeap(heap);
    qHeapFree(heap, qHeapAlloc(heap, maxAllocSize - 2));
    ValidateHeap(heap);
    qHeapFree(heap, qHeapAlloc(heap, maxAllocSize - 1));
    ValidateHeap(heap);
    qHeapFree(heap, qHeapAlloc(heap, maxAllocSize));
    ValidateHeap(heap);
    a = qHeapAlloc(heap, 100);
    ValidateHeap(heap);
    memset(a, 0x5b, 100);
    ValidateHeap(heap);
    b = qHeapAlloc(heap, 200);
    memset(b, 0x5b, 200);
    ValidateHeap(heap);
    c = qHeapAlloc(heap, 300);
    ValidateHeap(heap);
    memset(c, 0x5b, 300);
    ValidateHeap(heap);
    d = qHeapAlloc(heap, 200);
    ValidateHeap(heap);
    memset(d, 0x5b, 200);
    ValidateHeap(heap);
    qHeapFree(heap, d);
    ValidateHeap(heap);
    qHeapFree(heap, c);
    ValidateHeap(heap);
    c = qHeapAlloc(heap, 298);  /* realloc at the same spot, but bit less */
    ValidateHeap(heap);
    qHeapFree(heap, c);
    ValidateHeap(heap);
    qHeapFree(heap, a);
    ValidateHeap(heap);
    qHeapFree(heap, b);
    ValidateHeap(heap);
    a = qHeapAlloc(heap, 100);
    ValidateHeap(heap);
    memset(a, 0x5b, 100);
    ValidateHeap(heap);
    b = qHeapAlloc(heap, 200);
    memset(b, 0x5b, 200);
    ValidateHeap(heap);
    c = qHeapAlloc(heap, 300);
    ValidateHeap(heap);
    memset(c, 0x5b, 300);
    ValidateHeap(heap);
    d = qHeapAlloc(heap, 200);
    ValidateHeap(heap);
    memset(d, 0x5b, 200);
    ValidateHeap(heap);
    qHeapFree(heap, a);
    ValidateHeap(heap);
    qHeapFree(heap, c);
    ValidateHeap(heap);
    qHeapFree(heap, b);     /* merge from both sides */
    ValidateHeap(heap);
    qHeapFree(heap, d);     /* merge last with first (big) free */
    ValidateHeap(heap);
    a = qHeapAlloc(heap, maxAllocSize);
    memset(a, 0x6b, maxAllocSize);
    ValidateHeap(heap);
    qHeapFree(heap, a);
    ValidateHeap(heap);
    qHeapFree(heap, qHeapAlloc(heap, 0));
    a = qHeapAlloc(heap, 0);
    ValidateHeap(heap);
    qHeapFree(heap, a);
    ValidateHeap(heap);
    a = qHeapAlloc(heap, maxAllocSize + 1);
    qHeapFree(heap, a);
    ValidateHeap(heap);
    a = qHeapAlloc(heap, 100);
    ValidateHeap(heap);
    b = qHeapAlloc(heap, 200);
    ValidateHeap(heap);
    qHeapFree(heap, a);
    ValidateHeap(heap);
    a = qHeapAlloc(heap, 100);  /* get the same block again */
    ValidateHeap(heap);
    qHeapFree(heap, a);
    ValidateHeap(heap);
    a = qHeapAlloc(heap, heapSize);
    assert(!a);
    memset(ptrs, 0, numPtrs * sizeof(char *));
    printf("Static tests passed, starting random allocation test...\n");
    for (i = 0; i < numLoops; i++) {
        int orgCurPtr = rand() * (numPtrs - 1) / RAND_MAX, curPtr = orgCurPtr;
        /* randomly choose if we allocate or deallocate */
        if ((rand() > RAND_MAX / 2)) {
            int size = rand() * maxAllocSize / 4 / RAND_MAX;
            /* pick random index, and make sure it's pointing to a free spot */
            while (curPtr < numPtrs && ptrs[curPtr])
                curPtr++;
            if (curPtr >= numPtrs)
                for (curPtr = orgCurPtr - 1; curPtr >= 0 && ptrs[curPtr]; curPtr--);
            if (curPtr < 0 || curPtr >= numPtrs)
                continue;
            ptrs[curPtr] = (char *)qHeapAlloc(heap, size);
            if (ptrs[curPtr])
                memset(ptrs[curPtr], 0x5b, size);
            else {
                /* delete something */
                for (j = 0; j < numPtrs; j++) {
                    if (ptrs[j]) {
                        qHeapFree(heap, ptrs[j]);
                        ptrs[j] = NULL;
                        break;
                    }
                }
            }
        } else {
            qHeapFree(heap, ptrs[curPtr]);
            ptrs[curPtr] = NULL;
        }
        ValidateHeap(heap);
    }
    /* free all left-overs */
    for (i = 0; i < numPtrs; i++)
        qHeapFree(heap, ptrs[i]);
    checkMemoryLeaks(heap, heapSize);
    free(heap);
    free(ptrs);
    printf("qAlloc tests passed.\n");
    return 0;
}