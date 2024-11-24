#ifndef FT_MALLOC_H
#define FT_MALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#define FT_MALLOC_ALIGNMENT 16

#define FT_MALLOC_MIN_SIZE 32
// This is not a concrete limit, just an ideal virtual limit
#define FT_MALLOC_MAX_SIZE 0x7ffffffffffffff0

// #define FT_MALLOC_POISON_MEMORY
#define FT_MALLOC_MEMORY_PATTERN_NEVER_ALLOCATED 0xfe
#define FT_MALLOC_MEMORY_PATTERN_ALLOCATED_UNTOUCHED 0xce
#define FT_MALLOC_MEMORY_PATTERN_FREED 0xcd

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

static_assert(sizeof(size_t) == 8, "Expected size_t to be 64 bits");

#ifdef FT_MALLOC_BIG_SIZE_THRESHOLD
static_assert(FT_MALLOC_BIG_SIZE_THRESHOLD > 0, "Invalid value for FT_MALLOC_BIG_SIZE_THRESHOLD");
#endif

#ifdef FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD
static_assert(FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD > 0, "Invalid value for FT_MALLOC_BIG_SIZE_PAGE_THRESHOLD");
#endif

#ifdef FT_MALLOC_MIN_ALLOC_CAPACITY
static_assert(FT_MALLOC_MIN_ALLOC_CAPACITY > 0, "Invalid value for FT_MALLOC_MIN_ALLOC_CAPACITY");
#endif

typedef struct ListNode
{
    struct ListNode *prev;
    struct ListNode *next;
} ListNode;

typedef struct
{
    ListNode node; // 16 bytes
    size_t total_page_size; // 8 bytes
    size_t num_alloc_capacity; // 8 bytes
    size_t num_alloc; // 8 bytes
    size_t alloc_size; // 8 bytes
} AllocationBucket;

static_assert((sizeof(AllocationBucket) % FT_MALLOC_ALIGNMENT) == 0, "sizeof(AllocationBucket) is not a multiple of 16");

typedef struct
{
    ListNode node; // 16 bytes
    size_t size; // 8 bytes
    uint64_t alignment_padding; // Padding for 16-byte alignment of result pointer
} BigAllocationHeader;

static_assert((sizeof(BigAllocationHeader) % FT_MALLOC_ALIGNMENT) == 0, "sizeof(BigAllocationHeader) is not a multiple of 16");

typedef struct {
    AllocationBucket *allocation_bucket_list;
    BigAllocationHeader *big_allocation_list;
} MemoryHeap;

void *HeapAlloc(MemoryHeap *heap, size_t size);
void HeapFree(MemoryHeap *heap, void *ptr);
void *HeapResizeAlloc(MemoryHeap *heap, void *ptr, size_t new_size);
void DestroyHeap(MemoryHeap *heap);

extern MemoryHeap global_heap;

// We have these versions below just in case the linker resolves
// the wrong malloc/free/realloc and uses the one from libc, or
// if we also want to use libc's malloc e.g. for comparison
void *Alloc(size_t size);
void Free(void *ptr);
void *ResizeAlloc(void *ptr, size_t new_size);
void DestroyGlobalHeap();

typedef struct
{
    size_t num_buckets;
    size_t num_allocations;
    size_t num_allocated_bytes;
} AllocationBucketStats;

typedef struct
{
    size_t num_allocations;
    size_t num_allocated_bytes;
} BigAllocationStats;

typedef struct
{
    AllocationBucketStats bucket_stats;
    BigAllocationStats big_alloc_stats;
    size_t num_allocations;
    size_t num_allocated_bytes;
} AllocationStats;

AllocationStats GetHeapAllocationStats(MemoryHeap *heap);
AllocationStats GetAllocationStats();

void PrintHeapAllocationState(MemoryHeap *heap);
void PrintAllocationState();

// The subject requires a show_alloc_mem function that prints all allocations in ascending address order
void show_alloc_mem();

#ifdef FT_MALLOC_OVERRIDE_LIBC_MALLOC
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t new_size);
#endif

#endif
