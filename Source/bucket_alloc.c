// @Todo: store buckets in linked list per size class
// @Todo: use heuristics to know how big buckets should be

#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>

#include "malloc_internal.h"

static AllocationBucket *g_bucket_list;

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

AllocationBucket *CreateAllocationBucket(size_t alloc_size, unsigned int alloc_capacity)
{
    // Align page size to system page size
    size_t page_size = GetRequiredSizeForBucket(alloc_size, alloc_capacity);
    page_size = AlignToPageSize(page_size);

    // By aligning page size we might grow how much memory the bucket has
    // which means the alloc capacity might've increased, so we recalculate it
    alloc_capacity = GetBucketNumAllocCapacityBeforehand(page_size, alloc_size);

    void *ptr = mmap(NULL, page_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!ptr || ptr == MAP_FAILED)
        return NULL;

    AllocationBucket *bucket = (AllocationBucket *)ptr;
    *bucket = (AllocationBucket){};
    ListNodePushFront((ListNode **)&g_bucket_list, &bucket->node);

    bucket->alloc_size = alloc_size;
    bucket->total_page_size = page_size;

    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    size_t bookkeeping_size = GetBucketNumBookkeepingSlots(GetBucketNumAllocCapacity(bucket)) * sizeof(uint32_t);
    memset(bookkeeping, 0xffffffff, bookkeeping_size);

    DebugLog("Created allocation bucket: alloc_size=%lu, page_size=%lu, num_alloc_capacity=%lu\n",
        alloc_size, page_size, GetBucketNumAllocCapacity(bucket));

    return bucket;
}

void FreeAllocationBucket(AllocationBucket *bucket)
{
    Assert(bucket != NULL);
    Assert(bucket->num_alloc == 0 && "Freeing non empty allocation bucket");

    ListNodePop((ListNode **)&g_bucket_list, &bucket->node);

    DebugLog("Freed allocation bucket: alloc_size=%lu, page_size=%lu, num_alloc_capacity=%lu\n",
        bucket->alloc_size, bucket->total_page_size, GetBucketNumAllocCapacity(bucket));

    munmap(bucket, (size_t)bucket->total_page_size);
}

size_t GetBucketNumAllocCapacityBeforehand(size_t total_page_size, size_t alloc_size)
{
    size_t avail_without_bucket = total_page_size - sizeof(AllocationBucket);
    size_t num_alloc = (size_t)(avail_without_bucket / (alloc_size + 1.0 / sizeof(uint32_t)));

    while (sizeof(AllocationBucket) + GetBucketNumBookkeepingSlots(num_alloc) + num_alloc * alloc_size > total_page_size)
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
    size_t num_bookkeeping_slots = GetBucketNumBookkeepingSlots(GetBucketNumAllocCapacity(bucket));
    void *memory = (void *)(bookkeeping + num_bookkeeping_slots);

    DebugLog("OccupyFirstFreeBucketSlot(num_alloc=%lu, num_alloc_capacity=%lu, num_bookkeeping_slots=%lu)\n",
        bucket->num_alloc, GetBucketNumAllocCapacity(bucket), num_bookkeeping_slots);

    Assert(bucket->num_alloc < GetBucketNumAllocCapacity(bucket));

    for (size_t i = 0; i < num_bookkeeping_slots; i += 1)
    {
        // DebugLog("Iterating slot %lu\n", i);

        int bit_index = BitScanForward32(bookkeeping[i]);
        if (bit_index > 0)
        {
            DebugLog("Found free slot at i=%lu, bit_index=%d\n", i, bit_index);
            DebugLog("bookkeeping[%lu]=", i);
            DebugLogBinaryNumber(bookkeeping[i]);
            DebugLog("\n");
            DebugLog("bookkeeping[%lu], bit %d set to 0\n", i, bit_index);

            bit_index -= 1;
            bookkeeping[i] &= ~(1 << bit_index);

            size_t alloc_index = i * sizeof(uint32_t) + bit_index;
            Assert(alloc_index < GetBucketNumAllocCapacity(bucket));

            bucket->num_alloc += 1;

            return memory + alloc_index * bucket->alloc_size;
        }
    }

    Assert(false && "Code path should be unreachable");

    return NULL;
}

ssize_t GetPointerBucketAllocIndex(AllocationBucket *bucket, void *ptr)
{
    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    size_t num_bookkeeping_slots = GetBucketNumBookkeepingSlots(GetBucketNumAllocCapacity(bucket));

    void *memory_start = (void *)(bookkeeping + num_bookkeeping_slots);
    void *memory_end = (void *)bucket + bucket->total_page_size;

    if (ptr < memory_start || ptr > memory_end - bucket->alloc_size)
        return -1;

    ssize_t alloc_index = (ptr - memory_start) / bucket->alloc_size;

    return alloc_index;
}

void FreeBucketSlot(AllocationBucket *bucket, void *ptr)
{
    ssize_t alloc_index = GetPointerBucketAllocIndex(bucket, ptr);
    Assert(alloc_index >= 0 && "Pointer was not allocated from this bucket");
    size_t slot_index = (size_t)alloc_index / sizeof(uint32_t);
    size_t bit_index = (size_t)alloc_index % sizeof(uint32_t);

    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    bool is_already_free = (bookkeeping[slot_index] >> bit_index) & 1;
    Assert(!is_already_free && "Freeing memory that has already been freed or was never allocated");
    bookkeeping[slot_index] |= 1 << bit_index;
}

bool PointerWasAllocatedFromBucket(void *ptr, AllocationBucket *bucket)
{
    ssize_t alloc_index = GetPointerBucketAllocIndex(bucket, ptr);
    if (alloc_index < 0)
        return false;

    size_t slot_index = (size_t)alloc_index / sizeof(uint32_t);
    size_t bit_index = (size_t)alloc_index % sizeof(uint32_t);

    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    bool is_free = (bookkeeping[slot_index] >> bit_index) & 1;

    return !is_free;
}

AllocationBucket *GetAllocationBucketOfPointer(void *ptr)
{
    AllocationBucket *bucket = g_bucket_list;
    while (bucket)
    {
        if (PointerWasAllocatedFromBucket(ptr, bucket))
            return bucket;

        bucket = (AllocationBucket *)bucket->node.next;
    }

    return NULL;
}

AllocationBucket *GetAvailableAllocationBucketForSize(size_t size)
{
    size = AlignToNextPowerOfTwo(size);
    if (size < 32)
        size = 32;

    AllocationBucket *bucket = g_bucket_list;
    while (bucket)
    {
        if (size == bucket->alloc_size && bucket->num_alloc < GetBucketNumAllocCapacity(bucket))
            return bucket;

        bucket = (AllocationBucket *)bucket->node.next;
    }

    unsigned int alloc_capacity = FT_MALLOC_MIN_ALLOC_CAPACITY;

    return CreateAllocationBucket(size, alloc_capacity);
}

void *AllocFromBucket(size_t size)
{
    DebugLog(">> AllocFromBucket(%lu)\n", size);

    AllocationBucket *bucket = GetAvailableAllocationBucketForSize(size);
    if (!bucket)
        return NULL;

    void *ptr = OccupyFirstFreeBucketSlot(bucket);

    return ptr;
}

void FreeFromBucket(void *ptr)
{
    DebugLog(">> FreeFromBucket(%p)\n", ptr);

    AllocationBucket *bucket = GetAllocationBucketOfPointer(ptr);
    Assert(bucket != NULL && "Free: Invalid pointer");

    FreeBucketSlot(bucket, ptr);
}

void *ReallocFromBucket(void *ptr, size_t new_size)
{
    DebugLog(">> ReallocFromBucket(%p, %lu)\n", ptr, new_size);

    AllocationBucket *bucket = GetAllocationBucketOfPointer(ptr);
    Assert(bucket != NULL);

    new_size = AlignToNextPowerOfTwo(new_size);
    if (new_size < 32)
        new_size = 32;

    if (new_size <= bucket->alloc_size)
        return ptr;

    void *new_ptr = AllocFromBucket(new_size);
    memcpy(new_ptr, ptr, bucket->alloc_size);
    FreeBucketSlot(bucket, ptr);

    return new_ptr;
}

void PrintBucketAllocationState()
{
    EnsureInitialized();

    int total_num_buckets = 0;
    int total_num_allocations = 0;
    size_t total_num_allocated_bytes = 0;
    AllocationBucket *bucket = g_bucket_list;
    while (bucket)
    {
        total_num_buckets += 1;
        total_num_allocations += bucket->num_alloc;
        total_num_allocated_bytes += bucket->num_alloc * bucket->alloc_size;
        bucket = (AllocationBucket *)bucket->node.next;
    }

    printf("Total number of buckets: %d\n", total_num_buckets);
    printf("Total number of allocations: %d, %lu bytes\n", total_num_allocations, total_num_allocated_bytes);
    bucket = g_bucket_list;
    while (bucket)
    {
        printf(
            "Bucket(%p): alloc_size=%lu, total_page_size=%lu, num_alloc=%lu, num_alloc_capacity=%lu, num_allocated_bytes=%lu\n",
            bucket, bucket->alloc_size, bucket->total_page_size, bucket->num_alloc, GetBucketNumAllocCapacity(bucket), bucket->num_alloc * bucket->alloc_size
        );
        bucket = (AllocationBucket *)bucket->node.next;
    }
}
