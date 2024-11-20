#ifndef FT_MALLOC_INTERNAL_H
#define FT_MALLOC_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <sys/mman.h>
#include <unistd.h>

// If the allocation size goes beyong this many pages, we'll stop using buckets
// and store allocations directly in a linked list
#ifndef FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD
#define FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD 4
#endif

// Same as above but you can directly control how many bytes the threshold is
// #define FT_MALLOC_BIG_SIZE_THRESHOLD (4096 * 4)

#ifndef FT_MALLOC_MIN_ALLOC_CAPACITY
#define FT_MALLOC_MIN_ALLOC_CAPACITY 100
#endif

#ifdef FT_MALLOC_DEBUG_LOG
#define DebugLog(...) printf(__VA_ARGS__)
#else
#define DebugLog(...)
#endif

#define Stringify(x) Stringify2(x)
#define Stringify2(x) #x

#define AssertMsg(expr, line) __FILE__ ":" line ", assertion failed: " #expr "\n"
#define Assert(expr) do { if (!(expr)) { \
        write(2, AssertMsg(expr, Stringify(__LINE__)), sizeof(AssertMsg(expr, Stringify(__LINE__)))); \
        __builtin_trap(); \
    } } while(0)

extern size_t g_mmap_page_size;

void EnsureInitialized();

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
    return num_alloc_capacity / sizeof(uint32_t) + ((num_alloc_capacity % sizeof(uint32_t)) > 0);
}

AllocationBucket *CreateAllocationBucket(size_t alloc_size, size_t page_size);
void FreeAllocationBucket(AllocationBucket *bucket);

size_t GetBucketNumAllocCapacityBeforehand(size_t total_page_size, size_t alloc_size);
size_t GetBucketNumAllocCapacity(AllocationBucket *bucket);

void *OccupyFirstFreeBucketSlot(AllocationBucket *bucket);
void FreeBucketSlot(AllocationBucket *bucket, void *ptr);

bool PointerBelongsToBucket(void *ptr, AllocationBucket *bucket);
AllocationBucket *GetAllocationBucketOfPointer(void *ptr);
AllocationBucket *GetAvailableAllocationBucketForSize(size_t size);

void *AllocFromBucket(size_t size);
void FreeFromBucket(void *ptr);
void *ReallocFromBucket(void *ptr, size_t new_size);

typedef struct
{
    ListNode node;
    size_t size;
} BigAllocationHeader;

bool IsBigAllocation(void *ptr);
void *AllocBig(size_t size);
void FreeBig(void *ptr);
void *ReallocBig(void *ptr, size_t new_size);

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

static inline void *AlignPointer(void *ptr, int align)
{
    uint64_t addr = (uint64_t)ptr;
    if ((addr & align) != 0)
        addr = (addr + (align - 1)) & -align;

    return (void *)addr;
}

#endif
