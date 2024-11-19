#ifndef FT_MALLOC_INTERNAL_H
#define FT_MALLOC_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <sys/mman.h>
#include <unistd.h>

#ifdef FT_MALLOC_DEBUG_LOG
#define DebugLog(...) printf(__VA_ARGS__)
#else
#define DebugLog(...)
#endif

typedef struct ListNode
{
    struct ListNode *prev;
    struct ListNode *next;
} ListNode;

void ListNodeInsertAfter(ListNode *node, ListNode *after);
void ListNodeInsertBefore(ListNode *node, ListNode *before);
void ListNodePushFront(ListNode **list_front, ListNode *node);
void ListNodePop(ListNode **list_front, ListNode *node);

typedef struct
{
    ListNode node;
    uint16_t total_page_size;
    uint16_t num_alloc;
    uint16_t alloc_size;
} AllocationBucket;

static inline uint32_t *GetBucketBookkeepingDataPointer(AllocationBucket *bucket)
{
    return (uint32_t *)(bucket + 1);
}

static inline int GetBucketNumBookkeepingSlots(int num_alloc)
{
    return num_alloc / sizeof(uint32_t) + (num_alloc % sizeof(uint32_t)) > 0;
}

AllocationBucket *CreateAllocationBucket(int alloc_size, int page_size);
void FreeAllocationBucket(AllocationBucket *bucket);
int GetBucketNumAllocCapacity(AllocationBucket *bucket);

void *OccupyFirstFreeBucketSlot(AllocationBucket *bucket);
void FreeBucketSlot(AllocationBucket *bucket, void *ptr);

bool PointerBelongsToBucket(void *ptr, AllocationBucket *bucket);
AllocationBucket *GetAllocationBucketOfPointer(void *ptr);
AllocationBucket *GetAvailableAllocationBucketForSize(size_t size);

void *AllocFromBucket(size_t size);
void FreeFromBucket(void *ptr);
void *ReallocFromBucket(void *ptr, size_t new_size);

static inline uint32_t AlignToNextPowerOfTwo(uint32_t x)
{
    x -= 1;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x += 1;

    return x;
}

static inline int BitScanForward32(uint32_t value)
{
    return __builtin_ffsll((unsigned long long)value);
}

static inline int BitScanForward64(uint64_t value)
{
    return __builtin_ffsll(value);
}

#endif
