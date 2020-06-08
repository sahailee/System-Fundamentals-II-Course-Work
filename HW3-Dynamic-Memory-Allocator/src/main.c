#include <stdio.h>
#include "debug.h"
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    sf_mem_init();


    // sf_malloc(sizeof(int));
    // void *x = sf_memalign(50, 256);
    // // sf_show_heap();
    // // sf_show_free_lists();
    // // sf_show_blocks();
    // x += 1;
    // debug("%p\n", x);
    // debug("%p", sf_mem_start());;

    //printf("%f\n", *ptr);

    sf_mem_fini();

    return EXIT_SUCCESS;
}
