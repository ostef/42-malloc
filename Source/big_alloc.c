#include "malloc_internal.h"

// Big allocations are directly made using mmap, and recorded as a linked list

static BigAllocationHeader *g_alloc_list;

bool IsBigAllocation(void *ptr)
{
    BigAllocationHeader *alloc = g_alloc_list;
    while (alloc)
    {
        if (ptr == (void *)(alloc + 1))
            return true;

        alloc = (BigAllocationHeader *)alloc->node.next;
    }

    return false;
}

void *AllocBig(size_t size)
{
    void *ptr = mmap(NULL, size + sizeof(BigAllocationHeader), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    BigAllocationHeader *header = (BigAllocationHeader *)ptr;
    header->size = size;
    ListNodePushFront((ListNode **)&g_alloc_list, &header->node);

    ptr += sizeof(BigAllocationHeader);

    return ptr;
}

void FreeBig(void *ptr)
{
    ptr -= sizeof(BigAllocationHeader);
    BigAllocationHeader *header = (BigAllocationHeader *)ptr;
    ListNodePop((ListNode **)&g_alloc_list, &header->node);

    munmap(ptr, header->size + sizeof(BigAllocationHeader));
}

void *ReallocBig(void *ptr, size_t new_size)
{
    BigAllocationHeader *header = (BigAllocationHeader *)ptr - 1;

    size_t total_size = header->size + sizeof(BigAllocationHeader);
    int page_count = total_size / g_mmap_page_size + (total_size % g_mmap_page_size) != 0;

    size_t new_total_size = new_size + sizeof(BigAllocationHeader);
    int new_page_count = new_total_size / g_mmap_page_size + (new_total_size % g_mmap_page_size) != 0;

    // If the allocated pages are enough to store new_size bytes, just increase the recorded size and don't move the memory
    if (new_page_count == page_count)
    {
        header->size = new_size;
        return ptr;
    }

    void *new_ptr = AllocBig(new_size);
    memcpy(new_ptr, ptr, header->size);
    FreeBig(ptr);

    return new_ptr;
}
