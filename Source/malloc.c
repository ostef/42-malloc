#include <stdio.h>

#include "malloc.h"
#include "malloc_internal.h"

size_t g_mmap_page_size;

void EnsureInitialized()
{
    if (g_mmap_page_size <= 0)
        g_mmap_page_size = sysconf(_SC_PAGESIZE);
}

void *Allocate(size_t size)
{
    EnsureInitialized();

    #ifdef FT_MALLOC_BIG_SIZE_THRESHOLD
    Assert(FT_MALLOC_BIG_SIZE_THRESHOLD > 0 && "Invalid value for FT_MALLOC_BIG_SIZE_THRESHOLD");
    size_t big_size_threshold = (size_t)FT_MALLOC_BIG_SIZE_THRESHOLD;
    #else
    Assert(FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD > 0 && "Invalid value for FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD");
    size_t big_size_threshold = g_mmap_page_size * FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD;
    #endif

    if (size >= big_size_threshold)
        return AllocBig(size);
    else
        return AllocFromBucket(size);
}

void Free(void *ptr)
{
    EnsureInitialized();

    if (IsBigAllocation(ptr))
        FreeBig(ptr);
    else
        FreeFromBucket(ptr);
}

void *ResizeAllocation(void *ptr, size_t new_size)
{
    EnsureInitialized();

    if (IsBigAllocation(ptr))
        return ReallocBig(ptr, new_size);
    else
        return ReallocFromBucket(ptr, new_size);
}

#ifdef FT_MALLOC_OVERRIDE_LIBC_MALLOC

void *malloc(size_t size)
{
    return Allocate(size);
}

void free(void *ptr)
{
    Free(ptr);
}

void *realloc(void *ptr, size_t new_size)
{
    return ResizeAllocation(ptr, new_size);
}

#endif

// @Todo: replace printf (we're supposed to replace malloc, it would be
// weird to use functions that use malloc)
// Also we don't want to depend on stdio.h

void PrintAllocationState()
{
    printf("=== Small and medium allocations (bucket allocator) ===\n");
    PrintBucketAllocationState();

    printf("\n=== Big allocations ===\n");
    PrintBigAllocationState();
}
