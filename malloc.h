#ifndef FT_MALLOC_H
#define FT_MALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

static_assert(sizeof(size_t) == 8, "Expected size_t to be 64 bits");

#define FT_MALLOC_ALIGNMENT 16

#define FT_MALLOC_MIN_SIZE 32
// This is not a concrete limit, just an ideal virtual limit
#define FT_MALLOC_MAX_SIZE 0x7ffffffffffffff0

// #define FT_MALLOC_POISON_MEMORY
#define FT_MALLOC_MEMORY_PATTERN_NEVER_ALLOCATED 0xfe
#define FT_MALLOC_MEMORY_PATTERN_ALLOCATED_UNTOUCHED 0xce
#define FT_MALLOC_MEMORY_PATTERN_FREED 0xcd

// #ifndef FT_MALLOC_MIN_ALLOC_CAPACITY
// #define FT_MALLOC_MIN_ALLOC_CAPACITY 100
// #endif

#ifdef FT_MALLOC_MIN_ALLOC_CAPACITY
static_assert(FT_MALLOC_MIN_ALLOC_CAPACITY > 0, "Invalid value for FT_MALLOC_MIN_ALLOC_CAPACITY");
#endif

#define FT_MALLOC_SMALL_SIZE_GRANULARITY 16
#define FT_MALLOC_NUM_SMALL_SIZE_CLASS 100
#define FT_MALLOC_MIN_SMALL_SIZE FT_MALLOC_MIN_SIZE
#define FT_MALLOC_MAX_SMALL_SIZE (FT_MALLOC_MIN_SMALL_SIZE + FT_MALLOC_SMALL_SIZE_GRANULARITY * (FT_MALLOC_NUM_SMALL_SIZE_CLASS - 1))

#define FT_MALLOC_MID_SIZE_GRANULARITY 128
#define FT_MALLOC_NUM_MID_SIZE_CLASS 200
#define FT_MALLOC_MIN_MID_SIZE  (FT_MALLOC_MAX_SMALL_SIZE + 1)
#define FT_MALLOC_MAX_MID_SIZE (FT_MALLOC_MAX_SMALL_SIZE + FT_MALLOC_MID_SIZE_GRANULARITY * FT_MALLOC_NUM_MID_SIZE_CLASS)

#define FT_MALLOC_MIN_BIG_SIZE (FT_MALLOC_MAX_MID_SIZE + 1)

#define FT_MALLOC_NUM_SIZE_CLASS (FT_MALLOC_NUM_SMALL_SIZE_CLASS + FT_MALLOC_NUM_MID_SIZE_CLASS)

struct MemoryHeap *CreateHeap();
void DestroyHeap(struct MemoryHeap *heap);

void *HeapAlloc(struct MemoryHeap *heap, size_t size);
void *HeapRealloc(struct MemoryHeap *heap, void *ptr, size_t new_size);
void HeapFree(struct MemoryHeap *heap, void *ptr);

extern struct MemoryHeap *global_heap;

void *Alloc(size_t size);
void *Realloc(void *ptr, size_t new_size);
void Free(void *ptr);
void DestroyGlobalHeap();

typedef struct AllocationStats
{
    size_t num_allocated_bytes;
    size_t num_allocation_buckets;
    size_t num_allocations;
    size_t num_bucket_allocations;
    size_t num_big_allocations;
} AllocationStats;

AllocationStats GetAllocationStats();
void PrintAllocationState();

#endif
