#ifndef FT_MALLOC_INTERNAL_H
#define FT_MALLOC_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <sys/mman.h>
#include <unistd.h>

#include "malloc.h"

#define Stringify(x) Stringify2(x)
#define Stringify2(x) #x

#define AssertMsg(expr, line) __FILE__ ":" line ", assertion failed: " #expr "\n"
#define Assert(expr) do { if (!(expr)) { \
        write(2, AssertMsg(expr, Stringify(__LINE__)), sizeof(AssertMsg(expr, Stringify(__LINE__)))); \
        __builtin_trap(); \
    } } while(0)

#ifdef FT_MALLOC_DEBUG_LOG
#include <stdio.h>
#define DebugLog(...) printf(__VA_ARGS__)
#else
#define DebugLog(...)
#endif

extern size_t g_mmap_page_size;

void EnsureInitialized();

typedef struct ListNode
{
    struct ListNode *prev;
    struct ListNode *next;
} ListNode;

static inline void ListNodeInsertAfter(ListNode *node, ListNode *after)
{
    Assert(node->prev == NULL);
    Assert(node->next == NULL);

    ListNode *next = after->next;
    if (next)
    {
        next->prev = node;
        node->next = next;
    }

    node->prev = after;
    after->next = node;
}

static inline void ListNodeInsertBefore(ListNode *node, ListNode *before)
{
    Assert(node->prev == NULL);
    Assert(node->next == NULL);

    ListNode *prev = before->prev;
    if (prev)
    {
        prev->next = node;
        node->prev = prev;
    }

    node->next = before;
    before->prev = node;
}

static inline void ListNodePushFront(ListNode **list_front, ListNode *node)
{
    Assert(node->prev == NULL);
    Assert(node->next == NULL);

    if (*list_front)
    {
        node->next = *list_front;
        (*list_front)->prev = node;
    }

    *list_front = node;
}

static inline void ListNodePop(ListNode **list_front, ListNode *node)
{
    ListNode *prev = node->prev;
    ListNode *next = node->next;

    if (prev)
        prev->next = next;

    if (next)
        next->prev = prev;

    if (*list_front == node)
    {
        Assert(prev == NULL);
        *list_front = next;
    }

    node->prev = NULL;
    node->next = NULL;
}

typedef struct
{
    ListNode node;
    size_t total_page_size;
    size_t num_alloc;
    size_t alloc_size;
} AllocationBucket;

static inline uint32_t *GetBucketBookkeepingDataPointer(AllocationBucket *bucket)
{
    return (uint32_t *)(bucket + 1);
}

static inline size_t GetBucketNumBookkeepingSlots(size_t num_alloc_capacity)
{
    size_t num_bytes = num_alloc_capacity / 8 + (num_alloc_capacity % 8 > 0);
    size_t num_slots = num_bytes / sizeof(uint32_t) + (num_bytes % sizeof(uint32_t) > 0);

    return num_slots;
}

static inline size_t GetRequiredSizeForBucket(size_t alloc_size, unsigned int alloc_capacity)
{
    return sizeof(AllocationBucket) + GetBucketNumBookkeepingSlots(alloc_capacity) * sizeof(uint32_t) + alloc_size * alloc_capacity;
}

AllocationBucket *CreateAllocationBucket(size_t alloc_size, unsigned int alloc_capacity);
void FreeAllocationBucket(AllocationBucket *bucket);

size_t GetBucketNumAllocCapacityBeforehand(size_t total_page_size, size_t alloc_size);
size_t GetBucketNumAllocCapacity(AllocationBucket *bucket);

void *OccupyFirstFreeBucketSlot(AllocationBucket *bucket);
void FreeBucketSlot(AllocationBucket *bucket, void *ptr);

bool PointerBelongsToBucket(void *ptr, AllocationBucket *bucket, bool *already_freed);
AllocationBucket *GetAllocationBucketOfPointer(void *ptr, bool *already_freed);
AllocationBucket *GetAvailableAllocationBucketForSize(size_t size);

void *AllocFromBucket(size_t size);
void FreeFromBucket(void *ptr);
void *ReallocFromBucket(void *ptr, size_t new_size);
void CleanupBucketAllocations();

void PrintBucketAllocationState();

typedef struct
{
    ListNode node;
    size_t size;
} BigAllocationHeader;

bool IsBigAllocation(void *ptr);
void *AllocBig(size_t size);
void FreeBig(void *ptr);
void *ReallocBig(void *ptr, size_t new_size);
void CleanupBigAllocations();

void PrintBigAllocationState();

static inline uint64_t Align64BitNumberToNextPowerOfTwo(uint64_t x)
{
    if (x == 0)
        return 0;

    return 1ULL << (sizeof(uint64_t) * 8 - __builtin_clzll(x - 1));
}

static inline int BitScanForward32(uint32_t value)
{
    return __builtin_ffsll((unsigned long long)value);
}

static inline int BitScanForward64(uint64_t value)
{
    return __builtin_ffsll(value);
}

static inline void *AlignPointer(void *ptr, unsigned int align)
{
    uint64_t addr = (uint64_t)ptr;
    if ((addr & align) != 0)
        addr = (addr + (align - 1)) & ~(align - 1);

    return (void *)addr;
}

static inline size_t AlignToPageSize(size_t x)
{
    if ((x & g_mmap_page_size) != 0)
        x = (x + (g_mmap_page_size - 1)) & ~(g_mmap_page_size - 1);

    return x;
}

#endif
