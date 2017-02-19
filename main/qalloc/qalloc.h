#pragma once

extern "C" void qAllocInit();
extern "C" void qAllocFinish();

#ifdef DEBUG
#define qAlloc(size) qAlloc_(size, __FILE__, __LINE__)
#define qHeapAlloc(heap, size) qHeapAlloc_(heap, size, __FILE__, __LINE__)
void *qAlloc_(int size, const char *file, int line);
void *qHeapAlloc_(void *heap, int size, const char *file, int line);
void ValidateHeap(void *heap);
int GetMaxAllocSize(int heapSize);
void CheckMemoryLeaks(void *heap, int size);
extern "C" void ExcludeBlocks();
#else
void *qAlloc(int size);
void *qHeapAlloc(void *heap, int size);
#endif

void qFree(void *ptr);
void qHeapFree(void *heap, void *ptr);
void qHeapInit(void *heap, int size);
int qSetMinSplitSize(int splitSize);
