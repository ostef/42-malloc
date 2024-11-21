#include <stdio.h>

#include "malloc_internal.h"

// Big allocations are directly made using mmap, and recorded as a linked list

bool IsBigAllocation(MemoryHeap *heap, void *ptr)
{
    BigAllocationHeader *alloc = heap->big_allocation_list;
    while (alloc)
    {
        if (ptr == GetBigAllocPointer(alloc))
            return true;

        alloc = (BigAllocationHeader *)alloc->node.next;
    }

    return false;
}

void *AllocBig(MemoryHeap *heap, size_t size)
{
    DebugLog(">> AllocBig(%ld)\n", size);

    size_t page_size = AlignToPageSize(size + sizeof(BigAllocationHeader));
    void *ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    BigAllocationHeader *header = (BigAllocationHeader *)ptr;
    *header = (BigAllocationHeader){};
    header->size = size;

    ListNodePushFront((ListNode **)&heap->big_allocation_list, &header->node);

    ptr = GetBigAllocPointer(header);

#ifdef FT_MALLOC_POISON_MEMORY
    memset(ptr, FT_MALLOC_MEMORY_PATTERN_ALLOCATED_UNTOUCHED, size);
    memset(ptr + size, FT_MALLOC_MEMORY_PATTERN_NEVER_ALLOCATED, page_size - size - sizeof(BigAllocationHeader));
#endif

    return ptr;
}

void FreeBig(MemoryHeap *heap, void *ptr)
{
    DebugLog(">> FreeBig(%p)\n", ptr);

    BigAllocationHeader *header = GetBigAllocHeader(ptr);

    ListNodePop((ListNode **)&heap->big_allocation_list, &header->node);

    munmap((void *)header, header->size + sizeof(BigAllocationHeader));
}

void *ReallocBig(MemoryHeap *heap, void *ptr, size_t new_size)
{
    DebugLog(">> ReallocBig(%p, %lu)\n", ptr, new_size);

    BigAllocationHeader *header = GetBigAllocHeader(ptr);

    size_t total_size = header->size + sizeof(BigAllocationHeader);
    size_t page_count = total_size / GetPageSize() + ((total_size % GetPageSize()) != 0);

    size_t new_total_size = new_size + sizeof(BigAllocationHeader);
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

void CleanupBigAllocations(MemoryHeap *heap)
{
    while (heap->big_allocation_list)
    {
        FreeBig(heap, GetBigAllocPointer(heap->big_allocation_list));
    }
}

BigAllocationStats GetBigAllocationStats(MemoryHeap *heap)
{
    BigAllocationStats stats = {};
    BigAllocationHeader *alloc = heap->big_allocation_list;
    while (alloc)
    {
        stats.num_allocations += 1;
        stats.num_allocated_bytes += alloc->size;
        alloc = (BigAllocationHeader *)alloc->node.next;
    }

    return stats;
}

void PrintBigAllocationState(MemoryHeap *heap)
{
    int total_num_allocations = 0;
    size_t total_num_allocated_bytes = 0;
    BigAllocationHeader *alloc = heap->big_allocation_list;
    while (alloc)
    {
        total_num_allocations += 1;
        total_num_allocated_bytes += alloc->size;
        alloc = (BigAllocationHeader *)alloc->node.next;
    }

    printf("Total number of allocations: %d, %lu bytes\n", total_num_allocations, total_num_allocated_bytes);
    alloc = heap->big_allocation_list;
    while (alloc)
    {
        size_t total_size = alloc->size + sizeof(BigAllocationHeader);
        int page_count = total_size / GetPageSize() + ((total_size % GetPageSize()) != 0);
        printf(
            "Allocation(%p): %lu bytes, using %d pages\n",
            GetBigAllocPointer(alloc), alloc->size, page_count
        );

        alloc = (BigAllocationHeader *)alloc->node.next;
    }
}
