#include <stdio.h>

#include "malloc_internal.h"

// Big allocations are directly made using mmap, and recorded as a linked list

static BigAllocationHeader *g_alloc_list;

bool IsBigAllocation(void *ptr)
{
    BigAllocationHeader *alloc = g_alloc_list;
    while (alloc)
    {
        if (ptr == GetBigAllocPointer(alloc))
            return true;

        alloc = (BigAllocationHeader *)alloc->node.next;
    }

    return false;
}

void *AllocBig(size_t size)
{
    DebugLog(">> AllocBig(%ld)\n", size);

    void *ptr = mmap(NULL, size + sizeof(BigAllocationHeader), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    BigAllocationHeader *header = (BigAllocationHeader *)ptr;
    *header = (BigAllocationHeader){};
    header->size = size;

    ListNodePushFront((ListNode **)&g_alloc_list, &header->node);

    return GetBigAllocPointer(header);
}

void FreeBig(void *ptr)
{
    DebugLog(">> FreeBig(%p)\n", ptr);

    BigAllocationHeader *header = GetBigAllocHeader(ptr);

    ListNodePop((ListNode **)&g_alloc_list, &header->node);

    munmap((void *)header, header->size + sizeof(BigAllocationHeader));
}

void *ReallocBig(void *ptr, size_t new_size)
{
    DebugLog(">> ReallocBig(%p, %lu)\n", ptr, new_size);

    BigAllocationHeader *header = GetBigAllocHeader(ptr);

    size_t total_size = header->size + sizeof(BigAllocationHeader);
    size_t page_count = total_size / g_mmap_page_size + ((total_size % g_mmap_page_size) != 0);

    size_t new_total_size = new_size + sizeof(BigAllocationHeader);
    size_t new_page_count = new_total_size / g_mmap_page_size + ((new_total_size % g_mmap_page_size) != 0);

    // If the allocated pages are enough to store new_size bytes,
    // just change the recorded size and don't move the memory
    if (new_page_count <= page_count)
    {
        header->size = new_size;
        return ptr;
    }

    void *new_ptr = Allocate(new_size);
    size_t bytes_to_copy = header->size > new_size ? new_size : header->size;
    memcpy(new_ptr, ptr, bytes_to_copy);
    FreeBig(ptr);

    return new_ptr;
}

void CleanupBigAllocations()
{
    while (g_alloc_list)
    {
        FreeBig(GetBigAllocPointer(g_alloc_list));
    }
}

BigAllocationStats GetBigAllocationStats()
{
    EnsureInitialized();

    BigAllocationStats stats = {};
    BigAllocationHeader *alloc = g_alloc_list;
    while (alloc)
    {
        stats.num_allocations += 1;
        stats.num_allocated_bytes += alloc->size;
        alloc = (BigAllocationHeader *)alloc->node.next;
    }

    return stats;
}

void PrintBigAllocationState()
{
    EnsureInitialized();

    int total_num_allocations = 0;
    size_t total_num_allocated_bytes = 0;
    BigAllocationHeader *alloc = g_alloc_list;
    while (alloc)
    {
        total_num_allocations += 1;
        total_num_allocated_bytes += alloc->size;
        alloc = (BigAllocationHeader *)alloc->node.next;
    }

    printf("Total number of allocations: %d, %lu bytes\n", total_num_allocations, total_num_allocated_bytes);
    alloc = g_alloc_list;
    while (alloc)
    {
        size_t total_size = alloc->size + sizeof(BigAllocationHeader);
        int page_count = total_size / g_mmap_page_size + (total_size % g_mmap_page_size) != 0;
        printf(
            "Allocation(%p): %lu bytes, using %d pages\n",
            GetBigAllocPointer(alloc), alloc->size, page_count
        );

        alloc = (BigAllocationHeader *)alloc->node.next;
    }
}
