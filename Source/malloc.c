#include <stdio.h>

#include "malloc.h"
#include "malloc_internal.h"

MemoryHeap global_heap;

size_t GetPageSize()
{
    static size_t page_size = 0;

    if (page_size <= 0)
        page_size = sysconf(_SC_PAGESIZE);

    return page_size;
}

void *HeapAlloc(MemoryHeap *heap, size_t size)
{
    if (size > FT_MALLOC_MAX_SIZE)
        return NULL;

#ifdef FT_MALLOC_BIG_SIZE_THRESHOLD
    size_t big_size_threshold = (size_t)FT_MALLOC_BIG_SIZE_THRESHOLD;
#else
    size_t big_size_threshold = GetPageSize() * FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD;
#endif

    void *result = NULL;
    if (size >= big_size_threshold)
        result = AllocBig(heap, size);
    else
        result = AllocFromBucket(heap, size);

    Assert(((uint64_t)result % FT_MALLOC_ALIGNMENT) == 0 && "Result pointer is not 16-byte aligned");

    return result;
}

void HeapFree(MemoryHeap *heap, void *ptr)
{
    if (!ptr)
        return;

    if (IsBigAllocation(heap, ptr))
        FreeBig(heap, ptr);
    else
        FreeFromBucket(heap, ptr);
}

void *HeapResizeAlloc(MemoryHeap *heap, void *ptr, size_t new_size)
{
    if (!ptr)
        return HeapAlloc(heap, new_size);

    if (new_size > FT_MALLOC_MAX_SIZE)
    {
        HeapFree(heap, ptr);
        return NULL;
    }

    if (IsBigAllocation(heap, ptr))
        return ReallocBig(heap, ptr, new_size);
    else
        return ReallocFromBucket(heap, ptr, new_size);
}

void DestroyHeap(MemoryHeap *heap)
{
    CleanupBucketAllocations(heap);
    CleanupBigAllocations(heap);
}

void *Alloc(size_t size)
{
    return HeapAlloc(&global_heap, size);
}

void Free(void *ptr)
{
    HeapFree(&global_heap, ptr);
}

void *ResizeAlloc(void *ptr, size_t new_size)
{
    return HeapResizeAlloc(&global_heap, ptr, new_size);
}

void DestroyGlobalHeap()
{
    DestroyHeap(&global_heap);
}

AllocationStats GetHeapAllocationStats(MemoryHeap *heap)
{
    AllocationStats stats = {};
    stats.bucket_stats = GetBucketAllocationStats(heap);
    stats.big_alloc_stats = GetBigAllocationStats(heap);
    stats.num_allocations = stats.bucket_stats.num_allocations
        + stats.big_alloc_stats.num_allocations;
    stats.num_allocated_bytes = stats.bucket_stats.num_allocated_bytes
        + stats.big_alloc_stats.num_allocated_bytes;

    return stats;
}

AllocationStats GetAllocationStats()
{
    return GetHeapAllocationStats(&global_heap);
}

// @Todo: replace printf (we're supposed to replace malloc, it would be
// weird to use functions that use malloc)
// Also we don't want to depend on stdio.h

void PrintHeapAllocationState(MemoryHeap *heap)
{
    printf("=== Small and medium allocations (bucket allocator) ===\n");
    PrintBucketAllocationState(heap);

    printf("\n=== Big allocations ===\n");
    PrintBigAllocationState(heap);
}

void PrintAllocationState()
{
    PrintHeapAllocationState(&global_heap);
}

void show_alloc_mem()
{
    PrintAllocationState();
}

#ifdef FT_MALLOC_OVERRIDE_LIBC_MALLOC

void *malloc(size_t size)
{
    return Alloc(size);
}

void free(void *ptr)
{
    Free(ptr);
}

void *realloc(void *ptr, size_t new_size)
{
    return ResizeAlloc(ptr, new_size);
}

#endif
