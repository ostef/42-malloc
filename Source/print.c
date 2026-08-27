#include "ft_malloc_internal.h"

#include <stdbool.h>
#include <string.h>
#include <unistd.h>

static
void PrintString(const char *str) {
    write(1, str, strlen(str));
}

static
char GetDigit(int digit) {
    if (digit < 10) {
        return '0' + digit;
    }
    return 'A' + digit - 10;
}

static
void PrintSize(size_t n, int base) {
    char buff[64];

    int num_digits = 0;
    size_t n2 = n;
    while (num_digits < 1 || n2 != 0) {
        num_digits += 1;
        n2 /= base;
    }

    for (int i = 0; i < num_digits; i += 1) {
        buff[num_digits - i - 1] = GetDigit(n % base);
        n /= base;
    }

    write(1, buff, num_digits);
}

static
void PrintPtr(void *ptr) {
    PrintString("0x");
    PrintSize((uintptr_t)ptr, 16);
}

static inline
size_t GetPageSize() {
    return sysconf(_SC_PAGESIZE);
}

static
void PrintBlockInfo(BlockHeader *block) {
    PrintString("  Block: (");
    PrintPtr(block);
    PrintString(") ");
    PrintPtr(block + 1);
    PrintString(" - ");
    PrintPtr((void *)(block + 1) + block->size);
    PrintString(", ");
    PrintSize(block->size, 10);
    PrintString(" bytes\n");
}

static
void PrintBucketInfo(Bucket *bucket, size_t *total_allocated, size_t *total_available) {
    PrintString("Bucket: num_pages=");
    PrintSize(bucket->num_pages, 10);
    PrintString(", highest_available_size=");
    PrintSize(bucket->highest_available_size, 10);
    PrintString("\n");

    BlockHeader *block = bucket->free_blocks;
    PrintString(" Free blocks:\n");
    while (block) {
        PrintBlockInfo(block);
        *total_available += block->size;
        block = block->next;
    }

    block = bucket->occupied_blocks;
    PrintString(" Occupied blocks:\n");
    while(block) {
        PrintBlockInfo(block);
        *total_allocated += block->size;
        block = block->next;
    }
}

static
void PrintHeapInfo(Heap *heap) {
    size_t total_available = 0;
    size_t total_allocated = 0;
    size_t num_pages = 0;

    PrintString("\nTiny buckets:\n");
    Bucket *bucket = heap->free_tiny_buckets;
    while (bucket) {
        PrintBucketInfo(bucket, &total_allocated, &total_available);
        num_pages += bucket->num_pages;
        bucket = bucket->next;
    }

    bucket = heap->full_tiny_buckets;
    while (bucket) {
        PrintBucketInfo(bucket, &total_allocated, &total_available);
        num_pages += bucket->num_pages;
        bucket = bucket->next;
    }

    PrintString("\nSmall buckets:\n");
    bucket = heap->free_small_buckets;
    while (bucket) {
        PrintBucketInfo(bucket, &total_allocated, &total_available);
        num_pages += bucket->num_pages;
        bucket = bucket->next;
    }

    bucket = heap->full_small_buckets;
    while (bucket) {
        PrintBucketInfo(bucket, &total_allocated, &total_available);
        num_pages += bucket->num_pages;
        bucket = bucket->next;
    }

    PrintString("\nLarge allocations:\n");
    BlockHeader *block = heap->large_allocations;
    while (block) {
        PrintBlockInfo(block);

        total_allocated += block->size;

        size_t total_block_size = block->size + sizeof(BlockHeader);
        num_pages += total_block_size / GetPageSize();

        block = block->next;
    }

    PrintString("\nTotal allocated: ");
    PrintSize(total_allocated, 10);
    PrintString(", available: ");
    PrintSize(total_available, 10);
    PrintString(", num pages: ");
    PrintSize(num_pages, 10);
    PrintString("\n");
}

void show_alloc_mem_better() {
    pthread_mutex_lock(&g_heap_mutex);
    PrintHeapInfo(&g_heap);
    pthread_mutex_unlock(&g_heap_mutex);
}

static
size_t PrintBlockInfoOrdered(BlockHeader *block) {
    PrintPtr(block + 1);
    PrintString(" - ");
    PrintPtr((void *)(block + 1) + block->size);
    PrintString(" : ");
    PrintSize(block->size, 10);
    PrintString(" bytes\n");

    return block->size;
}

static
ListNode *GetFirstNode(ListNode *first) {
    ListNode *block = first;
    while (block) {
        if (block < first) {
            first = block;
        }

        block = block->next;
    }

    return first;
}

static
ListNode *GetNextNode(ListNode *first, void *block) {
    ListNode *next_block = NULL;
    ListNode *curr = first;
    while (curr) {
        if ((uintptr_t)curr > (uintptr_t)block && (!next_block || (uintptr_t)curr < (uintptr_t)next_block)) {
            next_block = curr;
        }

        curr = curr->next;
    }

    return next_block;
}

static
size_t PrintBucketInfoOrdered(Bucket *bucket) {
    if (bucket->flags & Bucket_TinyFlag) {
        PrintString("TINY : ");
    } else {
        PrintString("SMALL : ");
    }

    PrintPtr(bucket);
    PrintString("\n");

    size_t total = 0;

    BlockHeader *curr = (BlockHeader *)GetFirstNode((ListNode *)bucket->occupied_blocks);
    while (curr) {
        total += PrintBlockInfoOrdered(curr);
        curr = (BlockHeader *)GetNextNode((ListNode *)bucket->occupied_blocks, curr);
    }

    return total;
}

static
void *GetFirstBucketOrLargeBlock(Heap *heap, bool *is_bucket) {
    void *a = GetFirstNode((ListNode *)heap->free_tiny_buckets);
    void *b = GetFirstNode((ListNode *)heap->full_tiny_buckets);
    void *c = GetFirstNode((ListNode *)heap->free_small_buckets);
    void *d = GetFirstNode((ListNode *)heap->full_small_buckets);

    void *first = a;
    first = first != NULL && (b == NULL || first < b) ? first : b;
    first = first != NULL && (c == NULL || first < c) ? first : c;
    first = first != NULL && (d == NULL || first < d) ? first : d;

    void *e = GetFirstNode((ListNode *)heap->large_allocations);
    if (first == NULL || (e != NULL && e < first)) {
        *is_bucket = false;
        first = e;
    } else {
        *is_bucket = first != NULL;
    }

    return first;
}

static
void *GetNextBucketOrLargeBlock(Heap *heap, void *ptr, bool *is_bucket) {
    void *a = GetNextNode((ListNode *)heap->free_tiny_buckets, ptr);
    void *b = GetNextNode((ListNode *)heap->full_tiny_buckets, ptr);
    void *c = GetNextNode((ListNode *)heap->free_small_buckets, ptr);
    void *d = GetNextNode((ListNode *)heap->full_small_buckets, ptr);

    void *next = a;
    next = next != NULL && (b == NULL || next < b) ? next : b;
    next = next != NULL && (c == NULL || next < c) ? next : c;
    next = next != NULL && (d == NULL || next < d) ? next : d;

    void *e = GetNextNode((ListNode *)heap->large_allocations, ptr);
    if (next == NULL || (e != NULL && e < next)) {
        *is_bucket = false;
        next = e;
    } else {
        *is_bucket = next != NULL;
    }

    return next;
}

static
void PrintHeapInfoOrdered(Heap *heap) {
    size_t total = 0;

    bool is_bucket = false;
    void *curr = GetFirstBucketOrLargeBlock(heap, &is_bucket);
    while (curr) {
        if (is_bucket) {
            total += PrintBucketInfoOrdered(curr);
        } else {
            PrintString("LARGE : ");
            PrintPtr(curr);
            PrintString("\n");

            total += PrintBlockInfoOrdered(curr);
        }

        curr = GetNextBucketOrLargeBlock(heap, curr, &is_bucket);
    }

    PrintString("Total : ");
    PrintSize(total, 10);
    PrintString(" bytes\n");
}

void show_alloc_mem() {
    pthread_mutex_lock(&g_heap_mutex);
    PrintHeapInfoOrdered(&g_heap);
    pthread_mutex_unlock(&g_heap_mutex);
}
