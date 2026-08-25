#ifndef FT_MALLOC_INTERNAL_H
#define FT_MALLOC_INTERNAL_H

#include <ft_malloc.h>

#include <stdint.h>
#include <stdbool.h>

#define FT_Stringify(x) FT_Stringify2(x)
#define FT_Stringify2(x) #x

#ifndef FT_MALLOC_ENABLE_ASSERTS

#define FT_Assert(expr)

#else

#define FT_AssertMsg(expr, line) __FILE__ ":" line ", assertion failed: " #expr "\n"
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

#define FT_MALLOC_MIN_SIZE 16
#define FT_MALLOC_ALIGNMENT 16

#ifndef FT_MALLOC_MIN_BLOCKS
#define FT_MALLOC_MIN_BLOCKS 100
#endif

#ifndef FT_MALLOC_TINY_LIMIT
#define FT_MALLOC_TINY_LIMIT 512
#endif

#ifndef FT_MALLOC_SMALL_LIMIT
#define FT_MALLOC_SMALL_LIMIT 4096
#endif

struct BlockHeader;
struct Bucket;
struct Heap;

typedef struct ListNode {
    struct ListNode *prev;
    struct ListNode *next;
} ListNode;

void ListNodePushFront(ListNode **list_front, ListNode *node);
void ListNodePushAfter(ListNode **list_front, ListNode *node, ListNode *after);
void ListNodePop(ListNode **list_front, ListNode *node);

#define ListPushFront(list, node) ListNodePushFront((ListNode **)list, (ListNode *)node)
#define ListPushAfter(list, node, after) ListNodePushAfter((ListNode **)list, (ListNode *)node, (ListNode *)after)
#define ListPop(list, node) ListNodePop((ListNode **)list, (ListNode *)node)

typedef struct BlockHeader {
    struct BlockHeader *prev;
    struct BlockHeader *next;
    struct Bucket *bucket;
    size_t size;
} BlockHeader;

enum {
    Bucket_TinyFlag = 1 << 0,
};

typedef struct Bucket {
    struct Bucket *prev;
    struct Bucket *next;
    BlockHeader *free_blocks;
    BlockHeader *occupied_blocks;
    uint32_t highest_available_size;
    uint32_t num_pages;
    uint32_t flags;
    uint32_t padding;
} Bucket;

typedef struct Heap {
    Bucket *free_tiny_buckets;
    Bucket *full_tiny_buckets;
    Bucket *free_small_buckets;
    Bucket *full_small_buckets;
    BlockHeader *large_allocations;
} Heap;

Bucket *CreateBucket(uint32_t num_pages);
void DestroyBucket(Bucket *bucket);

Bucket *CreateTinyBucket(Heap *heap);
void DestroyTinyBucket(Heap *heap, Bucket *bucket);

Bucket *CreateSmallBucket(Heap *heap);
void DestroySmallBucket(Heap *heap, Bucket *bucket);

Bucket *FindBucket(Bucket *list, size_t alloc_size);

void *AllocFromBucket(Bucket *bucket, size_t size);
void FreeFromBucket(BlockHeader *block);
bool ResizeAllocFromBucket(BlockHeader *block, size_t new_size);

void *AllocLarge(Heap *heap, size_t size);
void FreeLarge(Heap *heap, BlockHeader *block);

void *HeapAlloc(Heap *heap, size_t size);
void HeapFree(Heap *heap, void *ptr);

void PrintBlockInfo(BlockHeader *block);
void PrintBucketInfo(Bucket *bucket);
void PrintHeapInfo(Heap *heap);

#endif
