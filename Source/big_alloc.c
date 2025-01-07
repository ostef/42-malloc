#include "malloc_internal.h"

void *AllocBig(MemoryHeap *heap, size_t size)
{
    FT_DebugLog(">> AllocBig(%ld)\n", size);

    size_t page_size = AlignToPageSize(size + sizeof(AllocHeader));
    void *page = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    AllocHeader *header = (AllocHeader *)page;
    *header = (AllocHeader){};
    header->size = size;

    ListPushFront(&heap->big_allocs, header);

    void *ptr = (void *)(header + 1);

#ifdef FT_MALLOC_POISON_MEMORY
    memset(ptr, FT_MALLOC_MEMORY_PATTERN_ALLOCATED_UNTOUCHED, size);
    memset(ptr + size, FT_MALLOC_MEMORY_PATTERN_NEVER_ALLOCATED, page_size - size - sizeof(AllocHeader));
#endif

    return ptr;
}

void *ReallocBig(MemoryHeap *heap, void *ptr, size_t new_size)
{
    FT_DebugLog(">> ReallocBig(%p, %lu)\n", ptr, new_size);

    AllocHeader *header = (AllocHeader *)ptr - 1;

    size_t total_size = header->size + sizeof(AllocHeader);
    size_t page_count = total_size / GetPageSize() + ((total_size % GetPageSize()) != 0);

    size_t new_total_size = new_size + sizeof(AllocHeader);
    size_t new_page_count = new_total_size / GetPageSize() + ((new_total_size % GetPageSize()) != 0);

    // If the allocated pages are enough to store new_size bytes,
    // just change the recorded size and don't move the memory
    if (new_page_count <= page_count)
    {
#ifdef FT_MALLOC_POISON_MEMORY
        if (header->size > new_size)
            memset(ptr + new_size, FT_MALLOC_MEMORY_PATTERN_FREED, header->size - new_size);
#endif

        header->size = new_size;
        return ptr;
    }

    void *new_ptr = HeapAlloc(heap, new_size);
    size_t bytes_to_copy = header->size > new_size ? new_size : header->size;
    memcpy(new_ptr, ptr, bytes_to_copy);
    FreeBig(heap, ptr);

    return new_ptr;
}

void FreeBig(MemoryHeap *heap, void *ptr)
{
    FT_DebugLog(">> FreeBig(%p)\n", ptr);

    AllocHeader *header = (AllocHeader *)ptr - 1;

    ListPop(&heap->big_allocs, header);

    size_t page_size = AlignToPageSize(header->size + sizeof(AllocHeader));
    munmap((void *)header, page_size);
}

void CleanupBigAllocations(MemoryHeap *heap)
{
    FT_DebugLog(">> CleanupBigAllocations\n");

    while (heap->big_allocs)
    {
        FreeBig(heap, (void *)(heap->big_allocs + 1));
    }
}
