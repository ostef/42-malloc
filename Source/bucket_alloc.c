#include "malloc_internal.h"

static int GetSizeClass(size_t size)
{
    if (size >= FT_MALLOC_MIN_BIG_SIZE)
        return -1;

    if (size >= FT_MALLOC_MIN_MID_SIZE)
    {
        size -= FT_MALLOC_MIN_MID_SIZE;

        int index = (int)(size / FT_MALLOC_MID_SIZE_GRANULARITY);
        index += GetSizeClass(FT_MALLOC_MAX_SMALL_SIZE);

        return index;
    }

    if (size <= FT_MALLOC_MIN_SIZE)
        return 0;

    int index = 1 + (int)(size / FT_MALLOC_SMALL_SIZE_GRANULARITY);

    return index;
}

static size_t AlignSizeToSizeClass(size_t size)
{
    FT_Assert(size < FT_MALLOC_MIN_BIG_SIZE);

    if (size >= FT_MALLOC_MIN_MID_SIZE)
    {
        size -= FT_MALLOC_MIN_MID_SIZE;
        size_t align = FT_MALLOC_MID_SIZE_GRANULARITY;
        size = FT_MALLOC_MIN_MID_SIZE + ((size + align - 1) / align) * align;

        return size;
    }

    if (size <= FT_MALLOC_MIN_SIZE)
        return FT_MALLOC_MIN_SIZE;

    size -= FT_MALLOC_MIN_SIZE;
    size_t align = FT_MALLOC_SMALL_SIZE_GRANULARITY;
    size = FT_MALLOC_MIN_SIZE + ((size + align - 1) / align) * align;

    return size;
}

static AllocBucket **GetBucketList(MemoryHeap *heap, size_t size)
{
    return &heap->buckets_per_size_class[GetSizeClass(size)];
}

AllocBucket *CreateAllocBucket(MemoryHeap *heap, size_t size, size_t capacity)
{
    FT_DebugLog(">> CreateAllocBucket(%lu, %lu)\n", size, capacity);

    size = AlignSizeToSizeClass(size);
    size_t required_size = sizeof(AllocBucket) + (sizeof(AllocHeader) + size) * capacity;
    size_t page_size = AlignToPageSize(required_size);
    void *page = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    // Recalculate capacity because we may allocate more than needed
    capacity = (page_size - sizeof(AllocBucket)) / (sizeof(AllocHeader) + size);

    AllocBucket *bucket = (AllocBucket *)page;
    *bucket = (AllocBucket){};
    ListPushFront(GetBucketList(heap, size), bucket);

    bucket->alloc_capacity = capacity;
    bucket->alloc_size = size;

    AllocHeader *node = (AllocHeader *)(bucket + 1);
    bucket->free_blocks = node;

    AllocHeader *prev = NULL;
    for (size_t i = 0; i < bucket->alloc_capacity; i += 1)
    {
        if (prev)
            prev->next = node;
        node->prev = prev;

        node->size = bucket->alloc_size;
        node->bucket = bucket;

#if FT_MALLOC_POISON_MEMORY
        void *ptr = (void *)(node + 1);
        memset(ptr, FT_MALLOC_MEMORY_PATTERN_NEVER_ALLOCATED, bucket->alloc_size);
#endif

        if (i != bucket->alloc_capacity - 1)
            node->next = (AllocHeader *)((void *)(node + 1) + size);
        else
            node->next = NULL;

        prev = node;
        node = node->next;
    }

    return bucket;
}

void DestroyAllocBucket(MemoryHeap *heap, AllocBucket *bucket)
{
    size_t page_size = sizeof(AllocBucket) + (sizeof(AllocHeader) + bucket->alloc_size) * bucket->alloc_capacity;
    page_size = AlignToPageSize(page_size);
    ListPop(GetBucketList(heap, bucket->alloc_size), bucket);

    munmap(bucket, page_size);
}

static void *AllocFromBucket(AllocBucket *bucket)
{
    FT_DebugLog(">> AllocFromBucket(%lu)\n", bucket->alloc_size);

    FT_Assert(bucket->free_blocks != NULL);

    AllocHeader *header = bucket->free_blocks;
    ListPop(&bucket->free_blocks, header);

    ListPushFront(&bucket->occupied_blocks, header);

    void *ptr = (void *)(header + 1);

#if FT_MALLOC_POISON_MEMORY
    memset(ptr, FT_MALLOC_MEMORY_PATTERN_ALLOCATED_UNTOUCHED, bucket->alloc_size);
#endif

    return ptr;
}

void *BucketAlloc(MemoryHeap *heap, size_t size)
{
    FT_DebugLog(">> BucketAlloc(%lu)\n", size);

    AllocBucket *bucket = *GetBucketList(heap, size);
    while (bucket && bucket->free_blocks == NULL)
    {
        bucket = bucket->next;
    }

    if (!bucket)
    {
#ifdef FT_MALLOC_MIN_ALLOC_CAPACITY
        size_t capacity = FT_MALLOC_MIN_ALLOC_CAPACITY;
#else
        size_t capacity = size <= FT_MALLOC_MAX_SMALL_SIZE ? 100 : 10;
#endif
        bucket = CreateAllocBucket(heap, size, capacity);
    }

    return AllocFromBucket(bucket);
}

void *BucketRealloc(MemoryHeap *heap, void *ptr, size_t new_size)
{
    FT_DebugLog(">> BucketRealloc(%lu)\n", new_size);

    AllocHeader *header = ((AllocHeader *)ptr) - 1;
    AllocBucket *bucket = header->bucket;
    FT_Assert(bucket != NULL);

    int size_class = GetSizeClass(header->size);
    int new_size_class = GetSizeClass(new_size);
    if (size_class == new_size_class)
        return ptr;

    void *new_ptr = HeapAlloc(heap, new_size);
    size_t copy_size = new_size > header->size ? header->size : new_size;
    memcpy(new_ptr, ptr, copy_size);

    BucketFree(heap, ptr);

    return new_ptr;
}

void BucketFree(MemoryHeap *heap, void *ptr)
{
    FT_DebugLog(">> BucketFree()\n");

    (void)heap;
    AllocHeader *header = ((AllocHeader *)ptr) - 1;
    AllocBucket *bucket = header->bucket;
    FT_Assert(bucket != NULL);
    ListPop(&bucket->occupied_blocks, header);
    ListPushFront(&bucket->free_blocks, header);

#if FT_MALLOC_POISON_MEMORY
    memset(ptr, FT_MALLOC_MEMORY_PATTERN_FREED, bucket->alloc_size);
#endif
}

void CleanupBucketAllocations(MemoryHeap *heap)
{
    for (int i = 0; i < FT_MALLOC_NUM_SIZE_CLASS; i += 1)
    {
        AllocBucket *bucket = heap->buckets_per_size_class[i];
        while (bucket)
        {
            AllocBucket *next = bucket->next;
            DestroyAllocBucket(heap, bucket);

            bucket = next;
        }
    }
}
