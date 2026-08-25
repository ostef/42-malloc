#include "ft_malloc_internal.h"

#include <stdbool.h>
#include <string.h>
#include <unistd.h>

/*
void PrintBlockInfo(BlockHeader *block) {
    printf("  Block: (%p) %p - %p, %u bytes\n", block, block + 1, (void *)(block + 1) + block->size, (uint32_t)block->size);
}

void PrintBucketInfo(Bucket *bucket) {
    printf("Bucket: num_pages=%u, highest_available_size=%u\n", bucket->num_pages, bucket->highest_available_size);

    BlockHeader *block = bucket->free_blocks;
    printf(" Free blocks:\n");
    while (block) {
        PrintBlockInfo(block);
        block = block->next;
    }

    block = bucket->occupied_blocks;
    printf(" Occupied blocks:\n");
    while(block) {
        PrintBlockInfo(block);
        block = block->next;
    }
}

void PrintHeapInfo(Heap *heap) {
    printf("\nFree tiny buckets:\n");
    Bucket *bucket = heap->free_tiny_buckets;
    while (bucket) {
        PrintBucketInfo(bucket);
        bucket = bucket->next;
    }

    printf("\nFull tiny buckets:\n");
    bucket = heap->full_tiny_buckets;
    while (bucket) {
        PrintBucketInfo(bucket);
        bucket = bucket->next;
    }

    printf("\nFree small buckets:\n");
    bucket = heap->free_small_buckets;
    while (bucket) {
        PrintBucketInfo(bucket);
        bucket = bucket->next;
    }

    printf("\nFull small buckets:\n");
    bucket = heap->full_small_buckets;
    while (bucket) {
        PrintBucketInfo(bucket);
        bucket = bucket->next;
    }

    printf("\nLarge allocations:\n");
    BlockHeader *block = heap->large_allocations;
    while (block) {
        PrintBlockInfo(block);
        block = block->next;
    }
}
*/

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
        if ((uintptr_t)curr > (uintptr_t)block && !next_block) {
            next_block = curr;
        } else if ((uintptr_t)curr > (uintptr_t)block && (uintptr_t)curr < (uintptr_t)next_block) {
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

    void *next = a;
    next = b == NULL || next < b ? next : b;
    next = c == NULL || next < c ? next : c;
    next = d == NULL || next < d ? next : d;

    void *e = GetFirstNode((ListNode *)heap->large_allocations);
    if (e != NULL && e < next) {
        *is_bucket = false;
        next = e;
    } else {
        *is_bucket = next != NULL;
    }

    return next;
}

static
void *GetNextBucketOrLargeBlock(Heap *heap, void *ptr, bool *is_bucket) {
    void *a = GetNextNode((ListNode *)heap->free_tiny_buckets, ptr);
    void *b = GetNextNode((ListNode *)heap->full_tiny_buckets, ptr);
    void *c = GetNextNode((ListNode *)heap->free_small_buckets, ptr);
    void *d = GetNextNode((ListNode *)heap->full_small_buckets, ptr);

    void *next = a;
    next = b == NULL || next < b ? next : b;
    next = c == NULL || next < c ? next : c;
    next = d == NULL || next < d ? next : d;

    void *e = GetNextNode((ListNode *)heap->large_allocations, ptr);
    if (e != NULL && e < next) {
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
    PrintHeapInfoOrdered(&g_heap);
}
