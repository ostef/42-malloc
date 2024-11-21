// @Todo: store buckets in linked list per size class
// @Todo: use heuristics to know how big buckets should be

#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>

#include "malloc_internal.h"

#ifdef FT_MALLOC_DEBUG_LOG
static inline void DebugLogBinaryNumber(uint32_t x)
{
    int i = 0;
    while (i < 32)
    {
        DebugLog("%d", x & 1);
        x >>= 1;
        i += 1;
    }
}
#else
#define DebugLogBinaryNumber(x) (void)x
#endif

size_t GetBucketBookkeepingSize(size_t num_alloc_capacity)
{
    size_t num_bytes = num_alloc_capacity / 8 + (num_alloc_capacity % 8 > 0);
    size_t num_slots = num_bytes / sizeof(uint32_t) + (num_bytes % sizeof(uint32_t) > 0);

    return AlignNumber(num_slots * sizeof(uint32_t), FT_MALLOC_ALIGNMENT);
}

AllocationBucket *CreateAllocationBucket(MemoryHeap *heap, size_t alloc_size, unsigned int alloc_capacity)
{
    // Align page size to system page size
    size_t page_size = GetRequiredSizeForBucket(alloc_size, alloc_capacity);
    page_size = AlignToPageSize(page_size);
    Assert((page_size % GetPageSize()) == 0);

    // By aligning page size we might grow how much memory the bucket has
    // which means the alloc capacity might've increased, so we recalculate it
    while (sizeof(AllocationBucket) + GetBucketBookkeepingSize(alloc_capacity) + alloc_capacity * alloc_size < page_size)
    {
        alloc_capacity += 1;
    }

    void *ptr = mmap(NULL, page_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!ptr || ptr == MAP_FAILED)
        return NULL;

    AllocationBucket *bucket = (AllocationBucket *)ptr;
    *bucket = (AllocationBucket){};
    ListNodePushFront((ListNode **)&heap->allocation_bucket_list, &bucket->node);

    bucket->alloc_size = alloc_size;
    bucket->total_page_size = page_size;
    bucket->num_alloc_capacity = alloc_capacity;

    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    size_t bookkeeping_size = GetBucketNumBookkeepingSlots(bucket->num_alloc_capacity) * sizeof(uint32_t);
    memset(bookkeeping, 0xffffffff, bookkeeping_size);

    DebugLog("Created allocation bucket: alloc_size=%lu, page_size=%lu, num_alloc_capacity=%lu\n",
        alloc_size, page_size, bucket->num_alloc_capacity);

    return bucket;
}

void FreeAllocationBucket(MemoryHeap *heap, AllocationBucket *bucket)
{
    Assert(bucket != NULL);
    Assert(bucket->num_alloc == 0 && "Freeing non empty allocation bucket");

    ListNodePop((ListNode **)&heap->allocation_bucket_list, &bucket->node);

    DebugLog("Freed allocation bucket: alloc_size=%lu, page_size=%lu, num_alloc_capacity=%lu\n",
        bucket->alloc_size, bucket->total_page_size, bucket->num_alloc_capacity);

    munmap(bucket, (size_t)bucket->total_page_size);
}

void CleanupBucketAllocations(MemoryHeap *heap)
{
    while (heap->allocation_bucket_list)
    {
        heap->allocation_bucket_list->num_alloc = 0;
        FreeAllocationBucket(heap, heap->allocation_bucket_list);
    }
}

size_t GetBucketNumAllocCapacityBeforehand(size_t total_page_size, size_t alloc_size)
{
    size_t avail_without_bucket = total_page_size - sizeof(AllocationBucket);
    size_t num_alloc = (size_t)(avail_without_bucket / (alloc_size + 1.0 / sizeof(uint32_t)));

    while (sizeof(AllocationBucket) + GetBucketBookkeepingSize(num_alloc) + num_alloc * alloc_size > total_page_size)
    {
        num_alloc -= 1;
    }

    return num_alloc;
}

size_t GetBucketNumAllocCapacity(AllocationBucket *bucket)
{
    return GetBucketNumAllocCapacityBeforehand(bucket->total_page_size, bucket->alloc_size);
}

void *OccupyFirstFreeBucketSlot(AllocationBucket *bucket)
{
    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    size_t num_bookkeeping_slots = GetBucketNumBookkeepingSlots(bucket->num_alloc_capacity);
    void *memory = GetBucketAllocationStartPointer(bucket);

    DebugLog("OccupyFirstFreeBucketSlot(num_alloc=%lu, num_alloc_capacity=%lu, num_bookkeeping_slots=%lu)\n",
        bucket->num_alloc, bucket->num_alloc_capacity, num_bookkeeping_slots);

    Assert(bucket->num_alloc < bucket->num_alloc_capacity);

    for (size_t i = 0; i < num_bookkeeping_slots; i += 1)
    {
        // DebugLog("Iterating slot %lu\n", i);

        int bit_index = BitScanForward32(bookkeeping[i]);
        if (bit_index > 0)
        {
            bit_index -= 1;
            size_t alloc_index = i * sizeof(uint32_t) * 8 + bit_index;

            DebugLog("Found free slot: slot_index=%lu, bit_index=%d, alloc_index=%lu\n", i, bit_index, alloc_index);
            DebugLog("bookkeeping[%lu]=", i);
            DebugLogBinaryNumber(bookkeeping[i]);
            DebugLog("\n");
            DebugLog("bookkeeping[%lu], bit %d set to 0\n", i, bit_index);

            bookkeeping[i] &= ~(1 << bit_index);

            Assert(alloc_index < bucket->num_alloc_capacity);

            bucket->num_alloc += 1;

            return memory + alloc_index * bucket->alloc_size;
        }
    }

    Assert(false && "Code path should be unreachable");

    return NULL;
}

ssize_t GetPointerBucketAllocIndex(AllocationBucket *bucket, void *ptr)
{
    void *memory_start = GetBucketAllocationStartPointer(bucket);
    void *memory_end = memory_start + bucket->alloc_size * bucket->num_alloc_capacity;

    if (ptr < memory_start || ptr > memory_end - bucket->alloc_size)
        return -1;

    ssize_t alloc_index = (ptr - memory_start) / bucket->alloc_size;
    Assert(((ptr - memory_start) % bucket->alloc_size) == 0 && "Pointer belongs to bucket but is not a valid allocation result");

    return alloc_index;
}

void FreeBucketSlot(AllocationBucket *bucket, void *ptr)
{
    ssize_t alloc_index = GetPointerBucketAllocIndex(bucket, ptr);
    Assert(alloc_index >= 0 && "Pointer was not allocated from this bucket");
    size_t slot_index = (size_t)alloc_index / (sizeof(uint32_t) * 8);
    size_t bit_index = (size_t)alloc_index % (sizeof(uint32_t) * 8);

    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    bool is_already_free = (bookkeeping[slot_index] >> bit_index) & 1;
    Assert(!is_already_free && "Freeing memory that has already been freed or was never allocated");

    DebugLog("Freeing slot at i=%lu, bit_index=%lu\n", slot_index, bit_index);
    DebugLog("bookkeeping[%lu]=", slot_index);
    DebugLogBinaryNumber(bookkeeping[slot_index]);
    DebugLog("\n");

    bookkeeping[slot_index] |= 1 << bit_index;
    DebugLog("bookkeeping[%lu], bit %lu set to 1\n", slot_index, bit_index);

    Assert(bucket->num_alloc > 0);
    bucket->num_alloc -= 1;
}

bool PointerWasAllocatedFromBucket(void *ptr, AllocationBucket *bucket, bool *already_freed)
{
    ssize_t alloc_index = GetPointerBucketAllocIndex(bucket, ptr);
    if (alloc_index < 0)
        return false;

    size_t slot_index = (size_t)alloc_index / (sizeof(uint32_t) * 8);
    size_t bit_index = (size_t)alloc_index % (sizeof(uint32_t) * 8);

    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    bool is_free = (bookkeeping[slot_index] >> bit_index) & 1;
    if (already_freed)
    {
        *already_freed = is_free;
        return true;
    }

    return !is_free;
}

AllocationBucket *GetAllocationBucketOfPointer(MemoryHeap *heap, void *ptr, bool *already_freed)
{
    AllocationBucket *bucket = heap->allocation_bucket_list;
    while (bucket)
    {
        if (PointerWasAllocatedFromBucket(ptr, bucket, already_freed))
            return bucket;

        bucket = (AllocationBucket *)bucket->node.next;
    }

    if (already_freed)
        *already_freed = false;

    return NULL;
}

AllocationBucket *GetAvailableAllocationBucketForSize(MemoryHeap *heap, size_t size)
{
    size = Align64BitNumberToNextPowerOfTwo(size);
    if (size < FT_MALLOC_MIN_SIZE)
        size = FT_MALLOC_MIN_SIZE;

    AllocationBucket *bucket = heap->allocation_bucket_list;
    while (bucket)
    {
        if (size == bucket->alloc_size && bucket->num_alloc < bucket->num_alloc_capacity)
            return bucket;

        bucket = (AllocationBucket *)bucket->node.next;
    }

    unsigned int alloc_capacity = FT_MALLOC_MIN_ALLOC_CAPACITY;

    return CreateAllocationBucket(heap, size, alloc_capacity);
}

void *AllocFromBucket(MemoryHeap *heap, size_t size)
{
    DebugLog(">> AllocFromBucket(%lu)\n", size);

    AllocationBucket *bucket = GetAvailableAllocationBucketForSize(heap, size);
    if (!bucket)
        return NULL;

    void *ptr = OccupyFirstFreeBucketSlot(bucket);

    return ptr;
}

void FreeFromBucket(MemoryHeap *heap, void *ptr)
{
    DebugLog(">> FreeFromBucket(%p)\n", ptr);

    bool already_freed = false;
    AllocationBucket *bucket = GetAllocationBucketOfPointer(heap, ptr, &already_freed);
    Assert(bucket != NULL && "Free: Invalid pointer");

    #if FT_MALLOC_DEBUG_LOG
    if (already_freed)
    {
        ssize_t alloc_index = GetPointerBucketAllocIndex(bucket, ptr);
        size_t slot_index = (size_t)alloc_index / (sizeof(uint32_t) * 8);
        size_t bit_index = (size_t)alloc_index % (sizeof(uint32_t) * 8);

        DebugLog("Double free: alloc_index=%ld, slot_index=%lu, bit_index=%lu\n", alloc_index, slot_index, bit_index);

        uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
        DebugLog("bookkeeping[%lu]=", slot_index);
        DebugLogBinaryNumber(bookkeeping[slot_index]);
        DebugLog("\n");
    }
    #endif

    Assert(!already_freed && "Free: Double free");

    FreeBucketSlot(bucket, ptr);
}

void *ReallocFromBucket(MemoryHeap *heap, void *ptr, size_t new_size)
{
    DebugLog(">> ReallocFromBucket(%p, %lu)\n", ptr, new_size);

    AllocationBucket *bucket = GetAllocationBucketOfPointer(heap, ptr, NULL);
    Assert(bucket != NULL && "Realloc: Invalid pointer");

    new_size = Align64BitNumberToNextPowerOfTwo(new_size);
    if (new_size < FT_MALLOC_MIN_SIZE)
        new_size = FT_MALLOC_MIN_SIZE;

    if (new_size <= bucket->alloc_size)
        return ptr;

    void *new_ptr = HeapAlloc(heap, new_size);
    size_t bytes_to_copy = bucket->alloc_size > new_size ? new_size : bucket->alloc_size;
    memcpy(new_ptr, ptr, bytes_to_copy);
    FreeBucketSlot(bucket, ptr);

    return new_ptr;
}

AllocationBucketStats GetBucketAllocationStats(MemoryHeap *heap)
{
    AllocationBucketStats stats = {};
    AllocationBucket *bucket = heap->allocation_bucket_list;
    while (bucket)
    {
        stats.num_buckets += 1;
        stats.num_allocations += bucket->num_alloc;
        stats.num_allocated_bytes += bucket->num_alloc * bucket->alloc_size;
        bucket = (AllocationBucket *)bucket->node.next;
    }

    return stats;
}

void PrintBucketAllocationState(MemoryHeap *heap)
{
    int total_num_buckets = 0;
    int total_num_allocations = 0;
    size_t total_num_allocated_bytes = 0;
    AllocationBucket *bucket = heap->allocation_bucket_list;
    while (bucket)
    {
        total_num_buckets += 1;
        total_num_allocations += bucket->num_alloc;
        total_num_allocated_bytes += bucket->num_alloc * bucket->alloc_size;
        bucket = (AllocationBucket *)bucket->node.next;
    }

    printf("Total number of buckets: %d\n", total_num_buckets);
    printf("Total number of allocations: %d, %lu bytes\n", total_num_allocations, total_num_allocated_bytes);
    bucket = heap->allocation_bucket_list;
    while (bucket)
    {
        printf(
            "Bucket(%p): alloc_size=%lu, total_page_size=%lu, num_alloc=%lu, num_alloc_capacity=%lu, num_allocated_bytes=%lu\n",
            bucket, bucket->alloc_size, bucket->total_page_size, bucket->num_alloc, bucket->num_alloc_capacity, bucket->num_alloc * bucket->alloc_size
        );
        bucket = (AllocationBucket *)bucket->node.next;
    }
}
