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

    if (size > FT_MALLOC_MAX_SIZE)
        return NULL;

#ifdef FT_MALLOC_BIG_SIZE_THRESHOLD
    size_t big_size_threshold = (size_t)FT_MALLOC_BIG_SIZE_THRESHOLD;
#else
    size_t big_size_threshold = g_mmap_page_size * FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD;
#endif

    void *result = NULL;
    if (size >= big_size_threshold)
        result = AllocBig(size);
    else
        result = AllocFromBucket(size);

    Assert(((uint64_t)result % FT_MALLOC_ALIGNMENT) == 0 && "Result pointer is not 16-byte aligned");

    return result;
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

    if (new_size > FT_MALLOC_MAX_SIZE)
    {
        Free(ptr);
        return NULL;
    }

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

void CleanupAllocations()
{
    CleanupBucketAllocations();
    CleanupBigAllocations();
}

AllocationStats GetAllocationStats()
{
    AllocationStats stats = {};
    stats.bucket_stats = GetBucketAllocationStats();
    stats.big_alloc_stats = GetBigAllocationStats();
    stats.num_allocations = stats.bucket_stats.num_allocations
        + stats.big_alloc_stats.num_allocations;
    stats.num_allocated_bytes = stats.bucket_stats.num_allocated_bytes
        + stats.big_alloc_stats.num_allocated_bytes;

    return stats;
}

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

void show_alloc_mem()
{
    PrintAllocationState();
}
