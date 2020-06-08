#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"

void assert_free_block_count(size_t size, int count);
void assert_free_list_block_count(size_t size, int count);

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == (bp->header & BLOCK_SIZE_MASK))
		cnt++;
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

/*
 * Assert that the free list with a specified index has the specified number of
 * blocks in it.
 */
void assert_free_list_size(int index, int size) {
    int cnt = 0;
    sf_block *bp = sf_free_list_heads[index].body.links.next;
    while(bp != &sf_free_list_heads[index]) {
	cnt++;
	bp = bp->body.links.next;
    }
    cr_assert_eq(cnt, size, "Free list %d has wrong number of free blocks (exp=%d, found=%d)",
		 index, size, cnt);
}

Test(sf_memsuite_student, malloc_an_int, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	int *x = sf_malloc(sizeof(int));

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_free_block_count(0, 1);
	assert_free_block_count(3904, 1);
	assert_free_list_size(NUM_FREE_LISTS-1, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sf_memsuite_student, malloc_three_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	// We want to allocate up to exactly three pages.
	void *x = sf_malloc(3 * PAGE_SZ - ((1 << 6) - sizeof(sf_header)) - 64 - 2*sizeof(sf_header));

	cr_assert_not_null(x, "x is NULL!");
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sf_memsuite_student, malloc_too_large, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(PAGE_SZ << 16);

	cr_assert_null(x, "x is not NULL!");
	assert_free_block_count(0, 1);
	assert_free_block_count(65408, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sf_memsuite_student, free_quick, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(32);
	/* void *z = */ sf_malloc(1);

	sf_free(y);

	assert_free_block_count(0, 2);
	assert_free_block_count(64, 1);
	assert_free_block_count(3776, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_no_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(1);

	sf_free(y);

	assert_free_block_count(0, 2);
	assert_free_block_count(256, 1);
	assert_free_block_count(3584, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *w = */ sf_malloc(8);
	void *x = sf_malloc(200);
	void *y = sf_malloc(300);
	/* void *z = */ sf_malloc(4);

	sf_free(y);
	sf_free(x);

	assert_free_block_count(0, 2);
	assert_free_block_count(576, 1);
	assert_free_block_count(3264, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, freelist, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *u = sf_malloc(200);
	/* void *v = */ sf_malloc(300);
	void *w = sf_malloc(200);
	/* void *x = */ sf_malloc(500);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(700);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_free_block_count(0, 4);
	assert_free_block_count(256, 3);
	assert_free_block_count(1600, 1);
	assert_free_list_size(3, 3);
	assert_free_list_size(NUM_FREE_LISTS-1, 1);

	// First block in list should be the most recently freed block.
	int i = 3;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(bp, (char *)y - 2*sizeof(sf_header),
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)y - 2*sizeof(sf_header));
}

Test(sf_memsuite_student, realloc_larger_block, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(int));
	/* void *y = */ sf_malloc(10);
	x = sf_realloc(x, sizeof(int) * 20);

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 128, "Realloc'ed block size not what was expected!");

	assert_free_block_count(0, 2);
	assert_free_block_count(64, 1);
	assert_free_block_count(3712, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_splinter, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(int) * 20);
	void *y = sf_realloc(x, sizeof(int) * 16);

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char*)y - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 128, "Block size not what was expected!");

	// There should be only one free block of size 3840.
	assert_free_block_count(0, 1);
	assert_free_block_count(3840, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_free_block, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(double) * 8);
	void *y = sf_realloc(x, sizeof(int));

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char*)y - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 64, "Realloc'ed block size not what was expected!");

	// After realloc'ing x, we can return a block of size 64 to the freelist.
	// This block will go into the main freelist and be coalesced.
	assert_free_block_count(0, 1);
	assert_free_block_count(3904, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################


/*
* This test makes sure that when we are freeing a block and there is no wilderness
* that we do not turn this block into a wilderness block.
*/
Test(sf_memsuite_student, free_no_wilderness, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(sizeof(int));
	/* void *y = */ sf_malloc(3841); //Wilderness block should now be 0.

	sf_free(x); //Now we free x.

	assert_free_block_count(0, 1); // The only free block should be the one we just freed.
	assert_free_block_count(64, 1); //And its size is 64.
	assert_free_list_size(0, 1); //Not in wilderness list, should be in 0 list.
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

/*
* This test will be for memalign.
* What should happen?
* The payload should already be aligned to a 128 byte boundary.
* So no block is freed before this one. However there is enough space afterwards to split
* That block after should coalesce with wilderness block
*/
Test(sf_memsuite_student, memalign_nofree1_withfreew, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_memalign(78, 128);

	cr_assert_not_null(x, "x is NULL!");
	cr_assert((uintptr_t)x % 128 == 0, "x is not aligned to a 128 byte boundary!");

	assert_free_block_count(0, 1); //Should only be one free block
	assert_free_block_count(3840, 1); //Only 1 block of this size.
	assert_free_list_size(NUM_FREE_LISTS-1, 1); //And that block should be wilderness block.
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

/*
* This test will be for memalign.
* What should happen?
* The payload should not be already aligned to a 256 byte boundary.
* So a portion is freed before.
* Followed by an allocated 64 byte block which is aligned to 256 boundary.
* Then a free block that will coalesce with wilderness
* Wilderness block.
*/

Test(sf_memsuite_student, memalign_free1_withfree2, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void * t = sf_malloc(sizeof(int));
	t += 64;
	void *x = sf_memalign(50, 256);

	cr_assert_not_null(x, "x is NULL!");
	cr_assert((uintptr_t)x % 256 == 0, "x is not aligned to a 256 byte boundary!");

	assert_free_block_count(0, 2); //Should be two free block
	assert_free_block_count(256 - ((uintptr_t) t % 256), 1); //Only 1 block of this size.
	assert_free_block_count(3968 - 64 - 64 - (256 - ((uintptr_t) t % 256)), 1); //And 1 block like this.
	assert_free_list_size(NUM_FREE_LISTS-1, 1); //And one block as wilderness.
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

/*
* This test will be for memalign, but with a very large size and low alignment.
*/

Test(sf_memsuite_student, memalign_free1_nofree2, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	sf_malloc(sizeof(int)); // TO make sure x is not already aligned to 128 boundary.
	void *x = sf_memalign(3008, 128);

	cr_assert_not_null(x, "x is NULL!");
	cr_assert((uintptr_t)x % 128 == 0, "x is not aligned to a 256 byte boundary!");

	assert_free_block_count(0, 2); //Should be two free block
	assert_free_block_count(64, 1);
	assert_free_block_count(3968 - 64 - 64 - 3072, 1); //And 1 block like this.
	assert_free_list_size(NUM_FREE_LISTS-1, 1); //And one block as wilderness.
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

/*
* Simple malloc boundary case. size = 0.
*/
Test(sf_memsuite_student, malloc_zero, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(0);

	cr_assert_null(x, "x is not NULL!");
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
	assert_free_block_count(0, 1);
	assert_free_list_size(NUM_FREE_LISTS-1, 1); //Even though size is 0, everything should be initialized.
}

/*
* Realloc - pass in a pointer that is aligned to 64 byte boundary.
*/
Test(sf_memsuite_student, realloc_invalid_pointer, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;

	void *x = sf_malloc(10);

	x = sf_realloc((x - 8), 150);

	cr_assert_null(x, "x is not NULL!");
	cr_assert(sf_errno == EINVAL, "sf_errno is not set to EINVAL");

}

/*
* Align is not a power of 2.
*/
Test(sf_memsuite_student, memalign_not_power_of_2, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_memalign(128, 289);

	cr_assert_null(x, "x is not NULL!");
	cr_assert(sf_errno == EINVAL, "sf_errno is not set to EINVAL");
}

