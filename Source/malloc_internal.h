#ifndef FT_MALLOC_INTERNAL_H
#define FT_MALLOC_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h> // For static_assert

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

static inline void VerifyList(ListNode *list)
{
    while (list)
    {
        if (list->prev)
            Assert(list->prev->next == list);
        if (list->next)
            Assert(list->next->prev == list);

        list = list->next;
    }
}

static inline void ListNodePushFront(ListNode **list_front, ListNode *node)
{
    Assert(node->prev == NULL);
    Assert(node->next == NULL);

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
    Assert(*list_front != NULL);
    VerifyList(*list_front);
#endif
}

static inline void ListNodePop(ListNode **list_front, ListNode *node)
{
    if (node->prev)
        Assert(node->prev->next == node);
    if (node->next)
        Assert(node->next->prev == node);

    ListNode *prev = node->prev;
    ListNode *next = node->next;

#ifdef VERIFY_LIST
    Assert(*list_front != NULL);
    VerifyList(*list_front);
#endif

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

#ifdef VERIFY_LIST
    if (*list_front)
        VerifyList(*list_front);
#endif
}

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

size_t GetBucketBookkeepingSize(size_t alloc_capacity);

static inline size_t GetRequiredSizeForBucket(size_t alloc_size, unsigned int alloc_capacity)
{
    return sizeof(AllocationBucket) + GetBucketBookkeepingSize(alloc_capacity) + alloc_size * alloc_capacity;
}

AllocationBucket *CreateAllocationBucket(MemoryHeap *heap, size_t alloc_size, unsigned int alloc_capacity);
void FreeAllocationBucket(MemoryHeap *heap, AllocationBucket *bucket);

size_t GetBucketNumAllocCapacityBeforehand(size_t total_page_size, size_t alloc_size);
size_t GetBucketNumAllocCapacity(AllocationBucket *bucket);

static inline void *GetBucketAllocationStartPointer(AllocationBucket *bucket)
{
    void *ptr = (void *)bucket;
    ptr += sizeof(AllocationBucket);
    ptr += GetBucketBookkeepingSize(bucket->num_alloc_capacity);

    return ptr;
}

void *OccupyFirstFreeBucketSlot(AllocationBucket *bucket);
void FreeBucketSlot(AllocationBucket *bucket, void *ptr);

bool PointerBelongsToBucket(void *ptr, AllocationBucket *bucket, bool *already_freed);
AllocationBucket *GetAllocationBucketOfPointer(MemoryHeap *heap, void *ptr, bool *already_freed);
AllocationBucket *GetAvailableAllocationBucketForSize(MemoryHeap *heap, size_t size);

void *AllocFromBucket(MemoryHeap *heap, size_t size);
void FreeFromBucket(MemoryHeap *heap, void *ptr);
void *ReallocFromBucket(MemoryHeap *heap, void *ptr, size_t new_size);
void CleanupBucketAllocations(MemoryHeap *heap);

AllocationBucketStats GetBucketAllocationStats(MemoryHeap *heap);

void PrintBucketAllocationState(MemoryHeap *heap);

static inline BigAllocationHeader *GetBigAllocHeader(void *ptr)
{
    return (BigAllocationHeader *)ptr - 1;
}

static inline void *GetBigAllocPointer(BigAllocationHeader *alloc)
{
    return (void *)(alloc + 1);
}

bool IsBigAllocation(MemoryHeap *heap, void *ptr);
void *AllocBig(MemoryHeap *heap, size_t size);
void FreeBig(MemoryHeap *heap, void *ptr);
void *ReallocBig(MemoryHeap *heap, void *ptr, size_t new_size);
void CleanupBigAllocations(MemoryHeap *heap);
BigAllocationStats GetBigAllocationStats(MemoryHeap *heap);
void PrintBigAllocationState(MemoryHeap *heap);

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

static inline uint64_t AlignNumber(uint64_t x, uint64_t align)
{
    if ((x % align) != 0)
        x += align - x % align;

    return x;
}

static inline uint64_t AlignToPageSize(uint64_t x)
{
    return AlignNumber(x, g_mmap_page_size);
}

static inline void *AlignPointer(void *ptr, uint64_t align)
{
    return (void *)AlignNumber((uint64_t)ptr, align);
}

#endif
