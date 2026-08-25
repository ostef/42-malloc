#include <stdio.h>

#include <ft_malloc.h>

int main() {
    const size_t max_size = 4096;
    const int num_allocs = 100;

    // show_alloc_mem();
    // printf("\n===========================\n");

    // ft_malloc(10);

    // show_alloc_mem();
    // printf("\n===========================\n");

    // return 0;

    void *allocs[num_allocs] = {};

    show_alloc_mem();
    printf("\n===========================\n");

    for (int i = 0; i < 1000; i += 1) {
        // Show stats every 100 operations
        if (i % 100 == 0) {
            show_alloc_mem();
            printf("\n===========================\n");
        }

        int index = rand() % num_allocs;

        if (allocs[index] != NULL) {
            ft_free(allocs[index]);
            allocs[index] = NULL;
        } else {
            size_t size = rand() % max_size;
            allocs[index] = ft_malloc(size);
        }
    }

    for (int i = 0; i < num_allocs; i += 1) {
        ft_free(allocs[i]);
        allocs[i] = NULL;
    }

    show_alloc_mem();
    printf("\n===========================\n");

    return 0;
}
