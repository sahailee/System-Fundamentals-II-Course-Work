#include "debug.h"
#include "sfmm.h"
#include "sfhelperfunctions.h"

/* These are some helper functions that I use in sfmm.c */

/* Read the size and allocated fields from adress p */
size_t get_size(sf_block *bp) {
    return ((bp->header) & BLOCK_SIZE_MASK);
}

size_t get_alloc(sf_block *bp) {
    return ((bp->header) & THIS_BLOCK_ALLOCATED);
}

size_t get_palloc(sf_block *bp) {
    return ((bp->header) & PREV_BLOCK_ALLOCATED);
}

/* Given block ptr bp, compute address of next and previous blocks */
void *next_blkp(void *bp) {
    return (bp + get_size(bp));
}
void *prev_blkp(void *bp) {
    return (bp - (((sf_block *)bp)->prev_footer & BLOCK_SIZE_MASK));
}

void set_nextblkp_palloc(void *bp, size_t palloc) {
    sf_block *nbp = (sf_block *) next_blkp(bp);
    if((void *)nbp >= epilogue) {
        return;
    }
    nbp->header = PACK(get_size(nbp), palloc, get_alloc(nbp));
    set_footer(nbp, (nbp->header));
}

void set_footer(void *bp, size_t i) {
    ((sf_block *)(bp + get_size(bp)))->prev_footer = i;
}

int get_min_fib(int n) {
    int i = 3;
    int j = 5;
    int index = 4;
    while(n <= j) {
        int temp = i;
        i = j;
        j += temp;
    }
    return index;
}

int get_class_size(size_t asize) {
    asize = asize / M;
    if(asize == 1) {
        return 0;
    }
    else if(asize == (2)) {
        return 1;
    }
    else if(asize == (3)){
        return 2;
    }
    int fib_index = get_min_fib(asize);
    if(fib_index < NUM_FREE_LISTS - 1) {
        return fib_index - 1;
    }
    else {
        return NUM_FREE_LISTS - 2;
    }

}

void insert_to_free_list(void *bp) {
    if(get_size(bp) < M) {
        debug("%s\n", "ERROR: Trying to add a block less than 64");
        return;
    }
    /* Check to see if we are inserting the wilderness block */
    void *wilderness = sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next;
    // What the first if does is check that there is no wilderness block and that bp is the block
    // right before epilogue. If these are true then this block should become the wilderness block.
    if(((wilderness == &(sf_free_list_heads[NUM_FREE_LISTS - 1])) && (bp + get_size(bp) + ROW) >= epilogue)
     || next_blkp(bp) == wilderness) {
        insert_wilderness(bp);
        return;
    }
    int i = get_class_size(get_size(bp));
    ((sf_block *)bp)->body.links.next = sf_free_list_heads[i].body.links.next;
    ((sf_block *)bp)->body.links.prev = &sf_free_list_heads[i];
    sf_free_list_heads[i].body.links.next->body.links.prev = bp;
    sf_free_list_heads[i].body.links.next = bp;
    set_nextblkp_palloc(bp, 0);

}

/*
** Not Sure if i need this... I know if i use my regular coalesce after gorwing the memory it should
** coalesce with the wilderness block, but then it will place the wilderness block in another free list.
*/
void *sf_coalesce_wilderness(sf_block *bp) {
    size_t size = get_size(bp);
    sf_block * wilderness = sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next;
    if(wilderness == &(sf_free_list_heads[NUM_FREE_LISTS - 1])) {
        return bp;
    }
    size += get_size(wilderness);
    if(bp > wilderness) {
        wilderness->header = PACK(size, get_palloc(wilderness), 0);
        set_footer(bp, PACK(size, get_palloc(wilderness), 0));
        return wilderness;
    }
    else {
        bp->header = PACK(size, get_palloc(bp), 0);
        set_footer(wilderness, PACK(size, get_palloc(bp), 0));
        return bp;
    }

}

void insert_wilderness(sf_block *bp) {
    int i = NUM_FREE_LISTS - 1;
    bp = (sf_block *)sf_coalesce_wilderness(bp);
    bp->body.links.next = &(sf_free_list_heads[i]);
    bp->body.links.prev = &(sf_free_list_heads[i]);
    sf_free_list_heads[i].body.links.next = bp;
    sf_free_list_heads[i].body.links.prev = bp;
}

void remove_from_free_list(sf_block *bp) {
    if(get_alloc(bp) || get_size(bp) < M) {
        debug("%s\n", "Can not remove, block is allocated. Not a part of a free list.");
        return;
    }
    /* Remove from free list */
    bp->body.links.prev->body.links.next = bp->body.links.next;
    bp->body.links.next->body.links.prev = bp->body.links.prev;
}
