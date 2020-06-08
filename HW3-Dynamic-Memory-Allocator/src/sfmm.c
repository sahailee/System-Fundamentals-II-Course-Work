/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include "sfhelperfunctions.h"
#include <errno.h>

static void *prologue;


static sf_block *find_fit(size_t asize, sf_block* dummynode) {
    /* First Fit Search */
    sf_block *bp = dummynode->body.links.next;
    while(bp != dummynode) {
        //If this block is not allocated and asize <= this block' size:
        if(!get_alloc(bp) && (asize <= get_size(bp))) {
            return bp;
        }
        bp = bp->body.links.next;
    }
    return NULL; /* No fit */
}

void sf_split(void *bp, size_t tot_size, size_t data_size) {
    if(tot_size - data_size >= 64) { //Need to split free block. This may result in a block not a multiple of 64.
        ((sf_block *)bp)->header = PACK(data_size, get_palloc(bp), 1);
        set_footer(bp, PACK(data_size, get_palloc(bp), 1));
        void *nbp = (next_blkp(bp)); // bp now points to the new free block.
        ((sf_block *)nbp)->header = PACK((tot_size - data_size), 0x2, 0);    //Define the new size and 0 for alloc bit.
        set_footer(nbp, PACK((tot_size - data_size), 0x2, 0)); //and 1 for prev alloc
        /* Now place the new free block in its appropriate free list */
        insert_to_free_list(nbp);
        // set_nextblkp_palloc(nbp, 0);
    }
    else {
        ((sf_block *)bp)->header = PACK(tot_size, get_palloc(bp), 1);
        set_footer(bp, PACK(tot_size, get_palloc(bp), 1));
        set_nextblkp_palloc(bp, 0x2);
    }
}


static void place(void *bp, size_t asize) {
    remove_from_free_list(bp);
    size_t csize = get_size(bp);
    sf_split(bp, csize, asize);
}

static void set_epilogue() {
    epilogue = sf_mem_end() - ROW;
    ((sf_block *)epilogue)->header = (0x1);
    ((sf_block *)epilogue)->prev_footer = 0;
}

/* Initialize the prologue and the epilogue. */
static void initialize_heap() {
    prologue = (sf_mem_start() + (6 * ROW)); //Adding the 7 row padding. The 7th is implicit as we dont set
    ((sf_block *)prologue)->header = (M | 0x2 | 0x1); //prologues's prev_footer
    set_footer((prologue), PACK(M, 0x2, 0x1));
    set_epilogue();
    /* Prologue points to the start of block struct which is prev_footer.
    * This makes the payload aligned to 64 byte.
    */
    /* Setup the wilderness block */
    sf_block *wilderness = prologue + M;
    wilderness->header = (((epilogue - (void *)wilderness - ROW) | 0x2) & ~0x1);
    set_footer(wilderness, (((epilogue - (void *)wilderness - ROW) | 0x2) & ~0x1));
    /* Initialize the free lists */
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev =&sf_free_list_heads[i];
     }
     /* Place wilderness block in the last free list. */
     insert_wilderness(wilderness);

}

void *sf_malloc(size_t size) {
    void *bp;
    if(sf_mem_start() == sf_mem_end()) {
        bp = (sf_block *) sf_mem_grow();
        if(bp == NULL) {
            return NULL;
        }
        initialize_heap();
    }
    if(size == 0) {
        return NULL;
    }
    size_t asize; /* Adjusted Block Size */
    asize = size + ROW; /* Adding footer and header */
    if(asize % M != 0) {
        asize = M * ((asize / M) + 1); /* Make asize a multiple of 64 */
    }

    /* Find smallesst size class */
    int class_size = get_class_size(asize);
    for(int i = class_size; i < NUM_FREE_LISTS; i++) {
        /* Search this free list for a fit */
        if ((bp = find_fit(asize, &sf_free_list_heads[i])) != NULL) { /* first-fit search of the determined class. */
            place(bp, asize);
            return ((sf_block *)bp)->body.payload;
        }
    }
    /* No fit so get some more memory */
    bp = sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next; //Set bp to the wilderness block.
    while(get_size(bp) < asize) {
        void *temp = sf_mem_grow();
        if(temp == NULL) {
            return NULL;
        }
        /* The old epilogue should now become free and we should take this new free block and coalesce.... */
        ((sf_block *)epilogue)->header = PACK((sf_mem_end() - epilogue - ROW), 0, 0);
        insert_wilderness((sf_block *)epilogue);
        set_epilogue();
        set_footer(bp, ((sf_block *)bp)->header);
    }
    place(bp, asize); //This is a prob here bc in place we assume we have prev and next.
    return ((sf_block *)bp)->body.payload;
}

void sf_free(void *pp) {
    sf_block *bp = (sf_block *)(pp - (2 * ROW));
    if(pp == NULL || !((pp - sf_mem_start()) % 64 == 0) || (get_alloc(bp) == 0)) {
        debug("%s%d", "Failed the check in free, is not a mult of 64?:", !(*(unsigned int *)pp & 0x1B207));
        debug("%s%ld", "Block size: ", get_size(bp));
        abort();
    }
    else if ((void *)&(((sf_block *)bp)->prev_footer) >= epilogue ||
        ((void *)&(((sf_block *)bp)->header) <= prologue)) {
        abort();
    }
    else if(((get_palloc(bp) == 0)) && ((((sf_block *)(bp))->prev_footer) & THIS_BLOCK_ALLOCATED)) {
        abort();
    }
    size_t size = get_size(bp);
    bp->header = PACK(size, get_palloc(bp), 0);
    set_footer(bp, PACK(size, get_palloc(bp), 0));
    sf_coalesce(bp);

}

void *sf_coalesce(void *pp) {
    size_t size = get_size(pp);
    size_t prev_alloc = get_palloc(pp);
    size_t next_alloc = get_alloc((pp + size));
    if(prev_alloc && next_alloc) {
        //Need to find the right list and put in there.
        insert_to_free_list(pp);
        return pp;
    }
    else if(prev_alloc && !next_alloc) {
        if((pp + size) >= epilogue) {
            insert_to_free_list(pp);
            return pp;
        }
        remove_from_free_list(pp+size);
        size += get_size(pp + size); //This will coalesce when there are no other blocks.
        ((sf_block *)pp)->header = PACK(size, prev_alloc, 0);
        set_footer(pp, PACK(size, prev_alloc, 0));
        //Need to find the right list and put in there
        insert_to_free_list(pp);

    }
    else if(!prev_alloc && next_alloc) {
        sf_block *prev_pp = (sf_block *)(prev_blkp(pp));
        if((void *)prev_pp <= prologue) {
            insert_to_free_list(pp);
            return pp;
        }
        remove_from_free_list(prev_pp);
        size += get_size(prev_pp);
        prev_pp->header = PACK(size, get_palloc(prev_pp), 0);
        set_footer(pp, PACK(size, get_palloc(prev_pp), 0));
        pp = prev_pp;
        //Need to find the right list and put it in there.
        insert_to_free_list(pp);
    }
    else {
        sf_block *prev_pp = (sf_block *)(prev_blkp(pp));
        if(prev_pp == prologue && (pp + size) == epilogue) {
            insert_to_free_list(pp);
            return pp;
        }
        else if((void *)prev_pp <= prologue) {
            remove_from_free_list(pp+size);
            size += get_size(pp + size);
            ((sf_block *)pp)->header = PACK(size, prev_alloc, 0);
            set_footer(pp, PACK(size, prev_alloc, 0));
            //Need to find the right list and put in there
            insert_to_free_list(pp);
            return pp;
        }
        else if((void *)(pp + size) >= epilogue) {
            remove_from_free_list(prev_pp);
            size += get_size(prev_pp);
            prev_pp->header = PACK(size, get_palloc(prev_pp), 0);
            set_footer(prev_pp, PACK(size, get_palloc(prev_pp), 0));
            pp = prev_pp;
            //Need to find the right list and put it in there.
            insert_to_free_list(pp);
            return pp;
        }
        remove_from_free_list(prev_pp);
        remove_from_free_list(pp + size);
        size += get_size(pp + size) + get_size(prev_pp);
        prev_pp->header = PACK(size, get_palloc(prev_pp), 0);
        set_footer(prev_pp, PACK(size, get_palloc(prev_pp), 0));
        pp = prev_pp;
        //Need to find the right list and put in there.
        insert_to_free_list(pp);
    }
    return pp;
}


void *sf_realloc(void *pp, size_t rsize) {
    sf_block *bp = (sf_block *)(pp - (2 * ROW));
    if(pp == NULL || !((pp - sf_mem_start()) % 64 == 0) || (get_alloc(bp) == 0)) {
        sf_errno = EINVAL;
        return NULL;
    }
    else if ((void *)&(((sf_block *)bp)->prev_footer) >= epilogue ||
        ((void *)&(((sf_block *)bp)->header) <= prologue)) {
        sf_errno = EINVAL;
        return NULL;
    }
    else if(((get_palloc(bp) == 0)) && ((((sf_block *)(bp))->prev_footer) & THIS_BLOCK_ALLOCATED)) {
        sf_errno = EINVAL;
        return NULL;
    }
    size_t size = get_size(bp);
    if(size == 0) {
        sf_free(bp);
        return NULL;
    }
    if(rsize > size) {
        void *new_spot = sf_malloc(rsize);
        if (new_spot == NULL) return NULL;
        memcpy(((sf_block *)new_spot)->body.payload, bp->body.payload, size - ROW);
        sf_free(pp);
        return new_spot;
    }
    /* If rsize is not a multiple of 64 this will cause a problem. */
    rsize += ROW;
    if(rsize % M != 0) {
        rsize = M * ((rsize / M) + 1); /* Make asize a multiple of 64 */
    }
    if(rsize < size) {
        sf_split(bp, size, rsize);
        pp = bp->body.payload;
    }
    return pp;
}

void *sf_memalign(size_t size, size_t align) {
    if(align < M || (align & 1)) {
        sf_errno = EINVAL;
        return NULL;
    }
    if(size == 0) {
        return NULL;
    }
    void *pp = sf_malloc(size + M + align);
    if(pp == NULL) {
        return NULL; //Malloc will set errno
    }
    void *bp = pp - (2 * ROW);
    size_t blksize = get_size(bp);
    size_t diff = ((size_t)pp % align);
    if(diff != 0) {
        diff = align - diff;
        pp += diff;
        ((sf_block *)bp)->header = PACK(diff, get_palloc(bp), 1);
        bp += diff;
        ((sf_block *)bp)->header = PACK(blksize - diff, 0, 1);
        bp -= diff;
        sf_free(((sf_block *)bp)->body.payload);
        bp += diff;
    }
    size_t asize; /* Adjusted Block Size */
    asize = size + ROW; /* Adding footer and header */
    if(asize % M != 0) {
        asize = M * ((asize / M) + 1); /* Make asize a multiple of 64 */
    }
    place(bp, asize);
    return (void *)(((sf_block *)bp)->body.payload);
}
