#ifndef FT_MALLOC_H
#define FT_MALLOC_H

#include <stddef.h>

#define FT_MALLOC_MIN_SIZE 32
#define FT_MALLOC_MAX_SIZE 0x7ffffffffffffff0

// If the allocation size goes beyong this many pages, we'll stop using buckets
// and store allocations directly in a linked list
#ifndef FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD
#define FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD 4
#endif

// Same as above but you can directly control how many bytes the threshold is
// #define FT_MALLOC_BIG_SIZE_THRESHOLD (4096 * 4)

#ifndef FT_MALLOC_MIN_ALLOC_CAPACITY
#define FT_MALLOC_MIN_ALLOC_CAPACITY 100
#endif

#ifdef FT_MALLOC_DEBUG_LOG
#include <stdio.h>
#define DebugLog(...) printf(__VA_ARGS__)
#else
#define DebugLog(...)
#endif

#ifdef FT_MALLOC_OVERRIDE_LIBC_MALLOC
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t new_size);
#endif

// We have these versions below just in case the linker resolves
// the wrong malloc/free/realloc and uses the one from libc, or
// if we also want to use libc's malloc e.g. for comparison
void *Allocate(size_t size);
void Free(void *ptr);
void *ResizeAllocation(void *ptr, size_t new_size);

void PrintAllocationState();

#endif
