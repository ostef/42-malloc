#ifndef FT_MALLOC_INTERNAL_H
#define FT_MALLOC_INTERNAL_H

#include "../malloc.h"

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define FT_Stringify(x) FT_Stringify2(x)
#define FT_Stringify2(x) #x

#define FT_AssertMsg(expr, line) __FILE__ ":" line ", assertion failed: " #expr "\n"

#ifdef FT_MALLOC_NO_ASSERT

#define FT_Assert(expr) (void)(expr)

#else

#define FT_Assert(expr) do { if (!(expr)) { \
        size_t n = write(2, FT_AssertMsg(expr, FT_Stringify(__LINE__)), sizeof(FT_AssertMsg(expr, FT_Stringify(__LINE__)))); \
        (void)n; \
        __builtin_trap(); \
    } } while(0)

#endif

#ifdef FT_MALLOC_DEBUG_LOG
#include <stdio.h>
#define FT_DebugLog(...) printf(__VA_ARGS__)
#else
#define FT_DebugLog(...)
#endif

typedef struct ListNode
{
    struct ListNode *prev;
    struct ListNode *next;
} ListNode;

void ListNodePushFront(ListNode **list_front, ListNode *node);
void ListNodePop(ListNode **list_front, ListNode *node);

#define ListPushFront(list, node) ListNodePushFront((ListNode **)list, (ListNode *)node)
#define ListPop(list, node) ListNodePop((ListNode **)list, (ListNode *)node)

typedef struct AllocBucket
{
    struct AllocBucket *prev;
    struct AllocBucket *next;
    size_t alloc_capacity;
    size_t alloc_size;
    struct AllocHeader *free_blocks;
    struct AllocHeader *occupied_blocks;
} AllocBucket;

static_assert(sizeof(AllocBucket) % 16 == 0, "AllocBucket is not aligned to 16 bytes");

typedef struct AllocHeader
{
    struct AllocHeader *prev;
    struct AllocHeader *next;
    AllocBucket *bucket;
    size_t size;
} AllocHeader;

static_assert(sizeof(AllocHeader) % 16 == 0, "AllocHeader is not aligned to 16 bytes");

typedef struct MemoryHeap
{
    AllocHeader *big_allocs;
    AllocBucket *buckets_per_size_class[FT_MALLOC_NUM_SIZE_CLASS];
} MemoryHeap;

void *AllocBig(MemoryHeap *heap, size_t size);
void *ReallocBig(MemoryHeap *heap, void *ptr, size_t new_size);
void FreeBig(MemoryHeap *heap, void *ptr);
void CleanupBigAllocations(MemoryHeap *heap);

AllocBucket *CreateAllocBucket(MemoryHeap *heap, size_t size, size_t capacity);
void DestroyAllocBucket(MemoryHeap *heap, AllocBucket *bucket);

void *BucketAlloc(MemoryHeap *heap, size_t size);
void *BucketRealloc(MemoryHeap *heap, void *ptr, size_t new_size);
void BucketFree(MemoryHeap *heap, void *ptr);

void CleanupBucketAllocations(MemoryHeap *heap);

size_t GetPageSize();

static inline uint64_t AlignNumber(uint64_t x, uint64_t align)
{
    if ((x % align) != 0)
        x += align - x % align;

    return x;
}

static inline uint64_t AlignToPageSize(uint64_t x)
{
    return AlignNumber(x, GetPageSize());
}

static inline void *AlignPointer(void *ptr, uint64_t align)
{
    return (void *)AlignNumber((uint64_t)ptr, align);
}

#endif
