#define M 64
#define ROW 8 /* Number of bytes in a row */
#define PACK(size, palloc, alloc) ((size) | (palloc) | (alloc))
void *epilogue;

size_t get_size(sf_block *bp);

size_t get_alloc(sf_block *bp);

size_t get_palloc(sf_block *bp);

void *next_blkp(void *bp);
void *prev_blkp(void *bp);

void set_footer(void *bp, size_t i);
void set_nextblkp_palloc(void *bp, size_t pallc);

void insert_to_free_list(void *bp);

void insert_wilderness(sf_block *bp);
void *sf_coalesce_wilderness(sf_block *bp);

void remove_from_free_list(sf_block *bp);

int get_min_fib(int n);

int get_class_size(size_t asize);
void *sf_coalesce(void *pp);
void sf_split(void *bp, size_t tot_size, size_t data_size);
