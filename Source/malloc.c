#include "ft_malloc_internal.h"

#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

Heap g_heap;
pthread_mutex_t g_heap_mutex = PTHREAD_MUTEX_INITIALIZER;

static void ListNodePushFront(ListNode **list_front, ListNode *node);
static void ListNodePushAfter(ListNode **list_front, ListNode *node, ListNode *after);
static void ListNodePop(ListNode **list_front, ListNode *node);

#define ListPushFront(list, node) ListNodePushFront((ListNode **)list, (ListNode *)node)
#define ListPushAfter(list, node, after) ListNodePushAfter((ListNode **)list, (ListNode *)node, (ListNode *)after)
#define ListPop(list, node) ListNodePop((ListNode **)list, (ListNode *)node)

static Bucket *CreateBucket(uint32_t num_pages);
static void DestroyBucket(Bucket *bucket);

static Bucket *CreateTinyBucket(Heap *heap);
static void DestroyTinyBucket(Heap *heap, Bucket *bucket);

static Bucket *CreateSmallBucket(Heap *heap);
static void DestroySmallBucket(Heap *heap, Bucket *bucket);

static Bucket *FindBucket(Bucket *list, size_t alloc_size);

static void *AllocFromBucket(Bucket *bucket, size_t size);
static void FreeFromBucket(BlockHeader *block);
static bool ResizeAllocFromBucket(BlockHeader *block, size_t new_size);

static void *AllocLarge(Heap *heap, size_t size);
static void FreeLarge(Heap *heap, BlockHeader *block);

static void *HeapAlloc(Heap *heap, size_t size);
static void *HeapRealloc(Heap *heap, void *ptr, size_t new_size);
static void HeapFree(Heap *heap, void *ptr);

void *malloc(size_t size) {
    return ft_malloc(size);
}

void *calloc(size_t num_elements, size_t element_size) {
    return ft_calloc(num_elements, element_size);
}

void *realloc(void *ptr, size_t size) {
    return ft_realloc(ptr, size);
}

void free(void *ptr) {
    return ft_free(ptr);
}

void *ft_malloc(size_t size) {
    pthread_mutex_lock(&g_heap_mutex);
    void *ptr = HeapAlloc(&g_heap, size);
    pthread_mutex_unlock(&g_heap_mutex);

    return ptr;
}

void *ft_calloc(size_t num_elements, size_t element_size) {
    void *ptr = ft_malloc(num_elements * element_size);
    if (!ptr) {
        return NULL;
    }

    memset(ptr, 0, num_elements * element_size);

    return ptr;
}

void *ft_realloc(void *ptr, size_t size) {
    pthread_mutex_lock(&g_heap_mutex);
    void *result = HeapRealloc(&g_heap, ptr, size);
    pthread_mutex_unlock(&g_heap_mutex);

    return result;
}

void ft_free(void *ptr) {
    pthread_mutex_lock(&g_heap_mutex);
    HeapFree(&g_heap, ptr);
    pthread_mutex_unlock(&g_heap_mutex);
}

#ifdef VERIFY_LIST

static inline
void VerifyList(ListNode *list) {
    while (list) {
        if (list->prev) {
            FT_Assert(list->prev->next == list);
        }
        if (list->next) {
            FT_Assert(list->next->prev == list);
        }

        list = list->next;
    }
}

#endif

void ListNodePushFront(ListNode **list_front, ListNode *node) {
    FT_Assert(node->prev == NULL);
    FT_Assert(node->next == NULL);

#ifdef VERIFY_LIST
    if (*list_front) {
        VerifyList(*list_front);
    }
#endif

    if (*list_front) {
        node->next = *list_front;
        (*list_front)->prev = node;
    }

    *list_front = node;

#ifdef VERIFY_LIST
    FT_Assert(*list_front != NULL);
    VerifyList(*list_front);
#endif
}

void ListNodePushAfter(ListNode **list_front, ListNode *node, ListNode *after) {
    if (after == NULL) {
        ListNodePushFront(list_front, node);
        return;
    }

#ifdef VERIFY_LIST
    FT_Assert(*list_front != NULL);
    VerifyList(*list_front);
#endif

    node->next = after->next;
    if (node->next) {
        node->next->prev = node;
    }

    node->prev = after;
    after->next = node;

#ifdef VERIFY_LIST
    FT_Assert(*list_front != NULL);
    VerifyList(*list_front);
#endif
}

void ListNodePop(ListNode **list_front, ListNode *node) {
    ListNode *prev = node->prev;
    ListNode *next = node->next;

#ifdef VERIFY_LIST
    FT_Assert(*list_front != NULL);
    VerifyList(*list_front);
#endif

    if (prev) {
        prev->next = next;
    }

    if (next) {
        next->prev = prev;
    }

    if (*list_front == node) {
        FT_Assert(prev == NULL);
        *list_front = next;
    }

    node->prev = NULL;
    node->next = NULL;

#ifdef VERIFY_LIST
    if (*list_front) {
        VerifyList(*list_front);
    }
#endif
}

static inline
size_t GetPageSize() {
    return sysconf(_SC_PAGESIZE);
}

static inline
size_t AlignForward(size_t value, size_t alignment) {
    return ((value + alignment - 1) / alignment) * alignment;
}

Bucket *CreateBucket(uint32_t num_pages) {
    size_t total_size = num_pages * GetPageSize();
    Bucket *bucket = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (!bucket || bucket == MAP_FAILED) {
        return NULL;
    }

    FT_Assert((uintptr_t)bucket % FT_MALLOC_ALIGNMENT == 0);

    memset(bucket, 0, sizeof(Bucket));

    BlockHeader *block = (BlockHeader *)(bucket + 1);
    memset(block, 0, sizeof(BlockHeader));
    bucket->num_pages = num_pages;

    block->size = total_size - sizeof(Bucket) - sizeof(BlockHeader);
    block->bucket = bucket;

    bucket->highest_available_size = block->size;

    ListPushFront(&bucket->free_blocks, block);

    return bucket;
}

void DestroyBucket(Bucket *bucket) {
    size_t total_size = bucket->num_pages * GetPageSize();
    munmap(bucket, total_size);
}

static inline
Bucket *CreateBucketForMaxSize(uint32_t max_size, uint32_t num_min_blocks) {
    size_t total_size = sizeof(Bucket) + (max_size + sizeof(BlockHeader)) * num_min_blocks;
    uint32_t num_pages = AlignForward(total_size, GetPageSize()) / GetPageSize();

    return CreateBucket(num_pages);
}

Bucket *CreateTinyBucket(Heap *heap) {
    FT_DebugLog("Creating tiny bucket\n");

    Bucket *bucket = CreateBucketForMaxSize(FT_MALLOC_TINY_LIMIT, FT_MALLOC_MIN_BLOCKS);
    if (!bucket) {
        return NULL;
    }

    bucket->flags |= Bucket_TinyFlag;
    ListPushFront(&heap->free_tiny_buckets, bucket);

    return bucket;
}

void DestroyTinyBucket(Heap *heap, Bucket *bucket) {
    FT_DebugLog("Destroying tiny bucket\n");

    ListPop(&heap->free_tiny_buckets, bucket);
    DestroyBucket(bucket);
}

Bucket *CreateSmallBucket(Heap *heap) {
    FT_DebugLog("Creating small bucket\n");

    Bucket *bucket = CreateBucketForMaxSize(FT_MALLOC_SMALL_LIMIT, FT_MALLOC_MIN_BLOCKS);
    if (!bucket) {
        return NULL;
    }

    ListPushFront(&heap->free_small_buckets, bucket);

    return bucket;
}

void DestroySmallBucket(Heap *heap, Bucket *bucket) {
    FT_DebugLog("Destroying small bucket\n");

    ListPop(&heap->free_small_buckets, bucket);
    DestroyBucket(bucket);
}

Bucket *FindBucket(Bucket *list, size_t alloc_size) {
    Bucket *bucket = list;
    while (bucket) {
        if (bucket->highest_available_size >= alloc_size) {
            return bucket;
        }

        bucket = bucket->next;
    }

    return NULL;
}

#ifdef VERIFY_LIST

static
void EnsureBlocksAreLinear(BlockHeader *blocks) {
    BlockHeader *block = blocks;
    while (block && block->next) {
        FT_Assert(block < block->next);
        block = block->next;
    }
}

#endif

static
BlockHeader *CoalesceWithPrev(BlockHeader *block) {
    if (!block->prev) {
        return block;
    }

    BlockHeader *prev = block->prev;
    void *prev_end = (void *)(prev + 1) + prev->size;

    // Merge if both blocks are right next to each other
    if (prev_end == block) {
        prev->next = block->next;
        if (prev->next) {
            prev->next->prev = prev;
        }

        prev->size += sizeof(BlockHeader) + block->size;
        block = prev;
    }

    return block;
}

static
void PushFreeBlockAndCoalesce(Bucket *bucket, BlockHeader *block) {
    // Find the closest block
    BlockHeader *prev_block = block > bucket->free_blocks ? bucket->free_blocks : NULL;
    while (prev_block && prev_block < block) {
        if (!prev_block->next || prev_block->next > block) {
            break;
        }

        prev_block = prev_block->next;
    }

    ListPushAfter(&bucket->free_blocks, block, prev_block);

#ifdef VERIFY_LIST
    EnsureBlocksAreLinear(bucket->free_blocks);
#endif

    // Coalesce
    block = CoalesceWithPrev(block);
    if (block->next) {
        block = CoalesceWithPrev(block->next);
    }

    if (block->size > bucket->highest_available_size) {
        bucket->highest_available_size = block->size;
    }
}

static
void UpdateHighestAvailableSize(Bucket *bucket, size_t size_removed) {
    // Update highest available size
    if (size_removed < bucket->highest_available_size) {
        return;
    }

    bucket->highest_available_size = 0;

    BlockHeader *curr = bucket->free_blocks;
    while (curr) {
        if (curr->size > bucket->highest_available_size) {
            bucket->highest_available_size = curr->size;
        }

        curr = curr->next;
    }
}

static
void PopFreeBlock(Bucket *bucket, BlockHeader *block) {
    ListPop(&bucket->free_blocks, block);
    UpdateHighestAvailableSize(bucket, block->size);
}

static
bool SplitBlockIfBigEnough(Bucket *bucket, BlockHeader *block, size_t size) {
    if (block->size < size + sizeof(BlockHeader) + FT_MALLOC_MIN_SIZE) {
        return false;
    }

    FT_DebugLog("Splitting block %p, size is %lu bytes, requested %lu bytes\n", block, block->size, size);

    size_t original_block_size = block->size;

    BlockHeader *new_block = (void *)(block + 1) + size;
    memset(new_block, 0, sizeof(BlockHeader));

    new_block->bucket = bucket;
    new_block->size = block->size - size - sizeof(BlockHeader);

    block->size = size;

    FT_DebugLog("New block: %p, %lu bytes\n", new_block, new_block->size);

    ListPushAfter(&bucket->free_blocks, new_block, block);
#ifdef VERIFY_LIST
    EnsureBlocksAreLinear(bucket->free_blocks);
#endif
    UpdateHighestAvailableSize(bucket, original_block_size);

    return true;
}

void *AllocFromBucket(Bucket *bucket, size_t size) {
    FT_Assert(size % FT_MALLOC_ALIGNMENT == 0);

    if (!bucket->free_blocks) {
        return NULL;
    }
    if (bucket->highest_available_size < size) {
        return NULL;
    }

    BlockHeader *block = bucket->free_blocks;
    while (block && block->size < size) {
        block = block->next;
    }

    FT_Assert(block != NULL);

    SplitBlockIfBigEnough(bucket, block, size);

    PopFreeBlock(bucket, block);
    ListPushFront(&bucket->occupied_blocks, block);
    bucket->num_allocations += 1;

    return block + 1;
}

void FreeFromBucket(BlockHeader *block) {
    Bucket *bucket = block->bucket;
    FT_Assert(bucket != NULL);

    ListPop(&bucket->occupied_blocks, block);
    bucket->num_allocations -= 1;

    PushFreeBlockAndCoalesce(bucket, block);
}

bool ResizeAllocFromBucket(BlockHeader *block, size_t new_size) {
    Bucket *bucket = block->bucket;
    FT_Assert(bucket != NULL);

    if (new_size <= block->size) {
        return true;
    }

    BlockHeader *next_block = bucket->free_blocks;
    while (next_block && next_block < block) {
        next_block = next_block->next;
    }

    if (!next_block) {
        return false;
    }

    if (next_block != (void *)(block + 1) + block->size) {
        return false;
    }

    if (block->size + next_block->size + sizeof(BlockHeader) < new_size) {
        return false;
    }

    size_t remaining_size = new_size - block->size;
    SplitBlockIfBigEnough(bucket, next_block, remaining_size);

    PopFreeBlock(bucket, next_block);

    block->size = new_size;

    return true;
}

void *AllocLarge(Heap *heap, size_t size) {
    FT_DebugLog("AllocLarge: %lu\n", size);

    FT_Assert(size % FT_MALLOC_ALIGNMENT == 0);

    size_t total_size = AlignForward(size + sizeof(BlockHeader), GetPageSize());
    BlockHeader *block = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (!block || block == MAP_FAILED) {
        return NULL;
    }

    FT_Assert((uintptr_t)block % FT_MALLOC_ALIGNMENT == 0);

    memset(block, 0, sizeof(BlockHeader));
    block->size = total_size - sizeof(BlockHeader);

    ListPushFront(&heap->large_allocations, block);
    heap->num_large_allocations += 1;

    return block + 1;
}

void FreeLarge(Heap *heap, BlockHeader *block) {
    ListPop(&heap->large_allocations, block);
    heap->num_large_allocations -= 1;

    size_t total_size = block->size + sizeof(BlockHeader);
    munmap(block, total_size);
}

void *HeapAlloc(Heap *heap, size_t size) {
    if (size == 0) {
        return NULL;
    }

    FT_DebugLog("HeapAlloc: %lu bytes\n", size);

    size = size < FT_MALLOC_MIN_SIZE ? FT_MALLOC_MIN_SIZE : size;
    size = AlignForward(size, FT_MALLOC_ALIGNMENT);

    if (size > FT_MALLOC_SMALL_LIMIT) {
        void *result = AllocLarge(heap, size);
        FT_DebugLog("  result=%p\n", result);
        FT_Assert((uintptr_t)result % FT_MALLOC_ALIGNMENT == 0);

        return result;
    }

    Bucket *bucket = NULL;
    bool is_tiny = size <= FT_MALLOC_TINY_LIMIT;
    Bucket **free_bucket_list = is_tiny ? &heap->free_tiny_buckets : &heap->free_small_buckets;
    Bucket **full_bucket_list = is_tiny ? &heap->full_tiny_buckets : &heap->full_small_buckets;

    bucket = FindBucket(*free_bucket_list, size);
    if (!bucket) {
        bucket = is_tiny ? CreateTinyBucket(heap) : CreateSmallBucket(heap);
    }

    if (!bucket) {
        return NULL;
    }

    void *result = AllocFromBucket(bucket, size);
    if (!bucket->free_blocks) {
        ListPop(free_bucket_list, bucket);
        ListPushFront(full_bucket_list, bucket);
    }

    FT_DebugLog("  result=%p\n", result);
    FT_Assert((uintptr_t)result % FT_MALLOC_ALIGNMENT == 0);

    return result;
}

void *HeapRealloc(Heap *heap, void *ptr, size_t new_size) {
    FT_DebugLog("HeapRealloc: %p, %lu bytes\n", ptr, new_size);

    if (!ptr) {
        return HeapAlloc(heap, new_size);
    }

    if (new_size == 0) {
        HeapFree(heap, ptr);
        return NULL;
    }

    BlockHeader *block = (BlockHeader *)ptr - 1;

    new_size = new_size < FT_MALLOC_MIN_SIZE ? FT_MALLOC_MIN_SIZE : new_size;
    new_size = AlignForward(new_size, FT_MALLOC_ALIGNMENT);

    // When shrinking, only reallocate when the block was big and we shrink to less than half the size
    if (new_size < block->size) {
        if (block->bucket != NULL) {
            return ptr;
        }

        if (new_size > block->size / 2) {
            return ptr;
        }
    }

    if (block->bucket && ResizeAllocFromBucket(block, new_size)) {
        return ptr;
    }

    void *new_ptr = HeapAlloc(heap, new_size);
    if (new_ptr) {
        size_t copy_size = new_size < block->size ? new_size : block->size;
        memcpy(new_ptr, ptr, copy_size);
    }

    HeapFree(heap, ptr);

    return new_ptr;
}

void HeapFree(Heap *heap, void *ptr) {
    if (!ptr) {
        return;
    }

    FT_DebugLog("HeapFree: %p\n", ptr);

    BlockHeader *block = ((BlockHeader *)ptr) - 1;
    if (block->bucket) {
        Bucket *bucket = block->bucket;
        bool was_full = bucket->free_blocks == NULL;

        FreeFromBucket(block);

        if (was_full) {
            if (bucket->flags & Bucket_TinyFlag) {
                ListPop(&heap->full_tiny_buckets, bucket);
                ListPushFront(&heap->free_tiny_buckets, bucket);
            } else {
                ListPop(&heap->full_small_buckets, bucket);
                ListPushFront(&heap->free_small_buckets, bucket);
            }
        }

        // I wouldn't destroy buckets right away for efficiency but maybe
        // the subject expects me to, so I'm not taking any risks here
        if (bucket->num_allocations == 0) {
            if (bucket->flags & Bucket_TinyFlag) {
                DestroyTinyBucket(heap, bucket);
            } else {
                DestroySmallBucket(heap, bucket);
            }
        }
    } else {
        FreeLarge(heap, block);
    }
}
