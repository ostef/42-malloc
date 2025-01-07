#include "malloc_internal.h"

size_t GetPageSize()
{
    static size_t page_size = 0;

    if (page_size <= 0)
        page_size = sysconf(_SC_PAGESIZE);

    return page_size;
}

MemoryHeap *CreateHeap()
{
    MemoryHeap *heap = mmap(NULL, sizeof(MemoryHeap), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(heap, 0, sizeof(MemoryHeap));

    return heap;
}

void DestroyHeap(MemoryHeap *heap)
{
    CleanupBigAllocations(heap);
    CleanupBucketAllocations(heap);
    munmap((void *)heap, sizeof(MemoryHeap));
}

void *HeapAlloc(MemoryHeap *heap, size_t size)
{
    if (size > FT_MALLOC_MAX_SIZE)
        return NULL;
    if (size == 0)
        return NULL;

    if (size >= FT_MALLOC_MIN_BIG_SIZE)
        return AllocBig(heap, size);

    return BucketAlloc(heap, size);
}

void *HeapRealloc(MemoryHeap *heap, void *ptr, size_t new_size)
{
    if (new_size > FT_MALLOC_MAX_SIZE)
    {
        HeapFree(heap, ptr);
        return NULL;
    }

    if (new_size == 0)
    {
        HeapFree(heap, ptr);
        return NULL;
    }

    if (ptr == NULL)
        return HeapAlloc(heap, new_size);

    AllocHeader *header = (AllocHeader *)ptr - 1;
    if (header->bucket == NULL)
        return ReallocBig(heap, ptr, new_size);

    return BucketRealloc(heap, ptr, new_size);
}

void HeapFree(MemoryHeap *heap, void *ptr)
{
    if (ptr == NULL)
        return;

    AllocHeader *header = (AllocHeader *)ptr - 1;
    if (header->bucket == NULL)
        FreeBig(heap, ptr);
    else
        BucketFree(heap, ptr);
}

MemoryHeap *global_heap;

void *Alloc(size_t size)
{
    if (!global_heap)
        global_heap = CreateHeap();

    return HeapAlloc(global_heap, size);
}

void *Realloc(void *ptr, size_t new_size)
{
    if (!global_heap)
        global_heap = CreateHeap();

    return HeapRealloc(global_heap, ptr, new_size);
}

void Free(void *ptr)
{
    if (!global_heap)
        global_heap = CreateHeap();

    HeapFree(global_heap, ptr);
}

void DestroyGlobalHeap()
{
    DestroyHeap(global_heap);
}

static inline void VerifyList(ListNode *list)
{
    while (list)
    {
        if (list->prev)
            FT_Assert(list->prev->next == list);
        if (list->next)
            FT_Assert(list->next->prev == list);

        list = list->next;
    }
}

void ListNodePushFront(ListNode **list_front, ListNode *node)
{
    FT_Assert(node->prev == NULL);
    FT_Assert(node->next == NULL);

#ifdef VERIFY_LIST
    if (*list_front)
        VerifyList(*list_front);
#endif

    if (*list_front)
    {
        node->next = *list_front;
        (*list_front)->prev = node;
    }

    *list_front = node;

#ifdef VERIFY_LIST
    FT_Assert(*list_front != NULL);
    VerifyList(*list_front);
#endif
}

void ListNodePop(ListNode **list_front, ListNode *node)
{
    if (node->prev)
        FT_Assert(node->prev->next == node);
    if (node->next)
        FT_Assert(node->next->prev == node);

    ListNode *prev = node->prev;
    ListNode *next = node->next;

#ifdef VERIFY_LIST
    FT_Assert(*list_front != NULL);
    VerifyList(*list_front);
#endif

    if (prev)
        prev->next = next;

    if (next)
        next->prev = prev;

    if (*list_front == node)
    {
        FT_Assert(prev == NULL);
        *list_front = next;
    }

    node->prev = NULL;
    node->next = NULL;

#ifdef VERIFY_LIST
    if (*list_front)
        VerifyList(*list_front);
#endif
}

AllocationStats GetAllocationStats()
{
    return (AllocationStats){};
}

void PrintAllocationState()
{
}
