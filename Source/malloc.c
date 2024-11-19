#include "malloc.h"
#include "malloc_internal.h"

// static void *AllocateBig(size_t size)
// {
//     void *ptr = mmap(NULL, size + sizeof(size_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
//     *(size_t *)ptr = size;

//     return ptr + sizeof(size_t);
// }

// static void FreeBig(void *ptr)
// {
//     ptr -= sizeof(size_t);
//     size_t size = *(size_t *)ptr;
//     munmap(ptr, size);
// }

void *Allocate(size_t size)
{
    return AllocFromBucket(size);
}

void Free(void *ptr)
{
    FreeFromBucket(ptr);
}

void *ResizeAllocation(void *ptr, size_t new_size)
{
    return ReallocFromBucket(ptr, new_size);
}

#ifdef OVERRIDE_LIBC_MALLOC

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

void ListNodeInsertAfter(ListNode *node, ListNode *after)
{
    FT_ASSERT(node->prev == NULL);
    FT_ASSERT(node->next == NULL);

    ListNode *next = after->next;
    if (next)
    {
        next->prev = node;
        node->next = next;
    }

    node->prev = after;
    after->next = node;
}

void ListNodeInsertBefore(ListNode *node, ListNode *before)
{
    FT_ASSERT(node->prev == NULL);
    FT_ASSERT(node->next == NULL);

    ListNode *prev = before->prev;
    if (prev)
    {
        prev->next = node;
        node->prev = prev;
    }

    node->next = before;
    before->prev = node;
}

void ListNodePushFront(ListNode **list_front, ListNode *node)
{
    FT_ASSERT(node->prev == NULL);
    FT_ASSERT(node->next == NULL);

    if (*list_front)
    {
        node->next = *list_front;
        (*list_front)->prev = node;
    }

    *list_front = node;
}

void ListNodePop(ListNode **list_front, ListNode *node)
{
    ListNode *prev = node->prev;
    ListNode *next = node->next;

    if (prev)
        prev->next = next;

    if (next)
        next->prev = prev;

    if (*list_front == node)
    {
        FT_ASSERT(prev == NULL);
        *list_front = next;
    }

    node->prev = NULL;
    node->next = NULL;
}
