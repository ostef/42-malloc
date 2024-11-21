#ifndef COMMON_H
#define COMMON_H

#include "../malloc.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

static inline float ElapsedTimeMS(struct timespec start, struct timespec end)
{
    uint64_t elapsed_sec = (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec;

    return elapsed_sec / 1000000.0;
}

static inline size_t GetRandomAllocSize()
{
    static const size_t Sizes[] = {
        32, 64, 128, 512, 1024, 4096, 16839, 50000, 100000, 300000, 1 * 1024 * 1024, 10 * 1024 * 1024
    };
    static const int Num_Sizes = sizeof(Sizes) / sizeof(*Sizes);

    unsigned int index = (unsigned int)rand();
    index = index % Num_Sizes;

    return Sizes[index];
}

#endif
