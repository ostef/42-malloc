#include <stdlib.h>
#include <stdio.h>

#include "malloc_internal.h"

static int g_mmap_page_size;
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

AllocationBucket *CreateAllocationBucket(int alloc_size, int page_size)
{
    assert(alloc_size > 0);
    assert(page_size > 0);

    if (g_mmap_page_size <= 0)
        g_mmap_page_size = sysconf(_SC_PAGESIZE);

    // Align page size to system page size
    page_size = g_mmap_page_size * (page_size / g_mmap_page_size + 1);

    #ifdef FT_MALLOC_MIN_ALLOC_CAPACITY
    while (GetBucketNumAllocCapacityBeforehand(page_size, alloc_size) < FT_MALLOC_MIN_ALLOC_CAPACITY)
    {
        page_size += g_mmap_page_size;
    }
    #endif

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

    DebugLog("Created allocation bucket: alloc_size=%d, page_size=%d, num_alloc_capacity=%d\n",
        alloc_size, page_size, GetBucketNumAllocCapacity(bucket));

    return bucket;
}

void FreeAllocationBucket(AllocationBucket *bucket)
{
    assert(bucket != NULL);
    assert(bucket->num_alloc == 0);

    ListNodePop((ListNode **)&g_bucket_list, &bucket->node);

    munmap(bucket, (size_t)bucket->total_page_size);
}

int GetBucketNumAllocCapacityBeforehand(int total_page_size, int alloc_size)
{
    int avail_without_bucket = total_page_size - sizeof(AllocationBucket);
    int num_alloc = (int)(avail_without_bucket / (alloc_size + 1.0 / sizeof(uint32_t)));

    while ((int)sizeof(AllocationBucket) + GetBucketNumBookkeepingSlots(num_alloc) + num_alloc * alloc_size > total_page_size)
    {
        num_alloc -= 1;
    }

    return num_alloc;
}

int GetBucketNumAllocCapacity(AllocationBucket *bucket)
{
    return GetBucketNumAllocCapacityBeforehand(bucket->total_page_size, bucket->alloc_size);
}

void *OccupyFirstFreeBucketSlot(AllocationBucket *bucket)
{
    assert(bucket->num_alloc < GetBucketNumAllocCapacity(bucket));

    DebugLog("OccupyFirstFreeBucketSlot\n");

    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    int num_bookkeeping_slots = GetBucketNumBookkeepingSlots(GetBucketNumAllocCapacity(bucket));
    void *memory = (void *)(bookkeeping + num_bookkeeping_slots);

    for (int i = 0; i < num_bookkeeping_slots; i += 1)
    {
        int index = BitScanForward32(bookkeeping[i]);
        if (index > 0)
        {
            DebugLog("Found free slot at i=%d, bit_index=%d\n", i, index);
            DebugLog("bookkeeping[i]=");
            DebugLogBinaryNumber(bookkeeping[i]);
            DebugLog("\n");

            index -= 1;
            bookkeeping[i] &= ~(1 << index);
            bucket->num_alloc += 1;
            int alloc_index = i * sizeof(uint32_t) + index;
            assert(alloc_index < GetBucketNumAllocCapacity(bucket));

            return memory + alloc_index * bucket->alloc_size;
        }
    }

    assert(false && "Code path should be unreachable");

    return NULL;
}

int GetPointerBucketAllocIndex(AllocationBucket *bucket, void *ptr)
{
    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    int num_bookkeeping_slots = GetBucketNumBookkeepingSlots(GetBucketNumAllocCapacity(bucket));

    void *memory_start = (void *)(bookkeeping + num_bookkeeping_slots);
    void *memory_end = (void *)bucket + bucket->total_page_size;

    if (ptr < memory_start || ptr > memory_end - bucket->alloc_size)
        return -1;

    int alloc_index = (ptr - memory_start) / bucket->alloc_size;

    return alloc_index;
}

void FreeBucketSlot(AllocationBucket *bucket, void *ptr)
{
    int alloc_index = GetPointerBucketAllocIndex(bucket, ptr);
    assert(alloc_index >= 0 && "Pointer was not allocated from this bucket");
    int slot_index = alloc_index / sizeof(uint32_t);
    int bit_index = alloc_index % sizeof(uint32_t);

    uint32_t *bookkeeping = GetBucketBookkeepingDataPointer(bucket);
    bool is_already_free = (bookkeeping[slot_index] >> bit_index) & 1;
    assert(!is_already_free && "Freeing memory that has already been freed or was never allocated");
    bookkeeping[slot_index] |= 1 << bit_index;
}

bool PointerWasAllocatedFromBucket(void *ptr, AllocationBucket *bucket)
{
    int alloc_index = GetPointerBucketAllocIndex(bucket, ptr);
    if (alloc_index < 0)
        return false;

    int slot_index = alloc_index / sizeof(uint32_t);
    int bit_index = alloc_index % sizeof(uint32_t);

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

    if (g_mmap_page_size <= 0)
        g_mmap_page_size = sysconf(_SC_PAGESIZE);

    return CreateAllocationBucket(size, g_mmap_page_size);
}

void *AllocFromBucket(size_t size)
{
    DebugLog("AllocFromBucket(%lu)\n", size);

    AllocationBucket *bucket = GetAvailableAllocationBucketForSize(size);
    if (!bucket)
        return NULL;

    void *ptr = OccupyFirstFreeBucketSlot(bucket);

    return ptr;
}

void FreeFromBucket(void *ptr)
{
    DebugLog("AllocFromBucket(%p)\n", ptr);

    AllocationBucket *bucket = GetAllocationBucketOfPointer(ptr);
    assert(bucket != NULL && "Free: Invalid pointer");

    FreeBucketSlot(bucket, ptr);
}

void *ReallocFromBucket(void *ptr, size_t new_size)
{
    DebugLog("AllocFromBucket(%p, %lu)\n", ptr, new_size);

    AllocationBucket *bucket = GetAllocationBucketOfPointer(ptr);
    assert(bucket != NULL);

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
