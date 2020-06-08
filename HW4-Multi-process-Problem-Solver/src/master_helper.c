#include <stdlib.h>
#include "polya.h"
#include "master_helper.h"
#include "debug.h"


void write_problem(int fd, void *problemo) {
    FILE *f = fdopen(fd, "w");
    char *prob = (char *)problemo;
    size_t size = ((struct problem *)prob)->size;
    //debug("%ld", size);
    for(size_t i = 0; i < sizeof(struct problem); i++) {
        if(fputc(prob[i], f) == EOF) {
            debug("%s", "FPUTC returns EOF");
        }
    }
    for(int i = sizeof(struct problem); i < size; i++) {
        if(fputc(prob[i], f) == EOF) {
            debug("%s", "FPUTC returns EOF");
        }
    }
    fflush(f);
    //close(fd);

}


//If return is null then that should mean no result to be read.
void *read_result(int fd) {
    FILE *f = fdopen(fd, "r");
    char *res = malloc(sizeof(struct result));
    if(res == NULL) {
        debug("%s", "malloc returned NULL.");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < sizeof(struct result); i++) {
        char c = fgetc(f);
        if(c == EOF) {
            debug("%s", "FGETC returns EOF");
        }
        res[i] = c;
    }
    // if(((struct result *) res)->failed != 0) {
    //     //fflush(f);
    //     return res; //No need to read the data involved.
    // }
    size_t size = ((struct result *) res)->size;
    res = realloc(res, size);
    if(res == NULL) {
        debug("%s", "realloc returned NULL.");
        exit(EXIT_FAILURE);
    }
    char *res_data = ((struct result *)res)->data;
    size -= sizeof(struct result);
    for(int i = 0; i < size; i++) {
        char c = fgetc(f);
        if(c == EOF) {
            debug("%s", "FGETC returns EOF");
        }
        res_data[i] = c;
    }
    return res;
}

int get_index_for_pid(pid_t cid, int size, pid_t *cpids) {
    for(int i = 0; i < size; i++) {
        if(cpids[i] == cid) {
            return i;
        }
    }
    return -1;
}

void close_all_fds(int workers, int fds[workers][2]) {
    for(int i = 0; i < workers; i++) {
        for(int j = 0; j < 2; j++) {
            if(close(fds[i][j]) == -1) {
                debug("%s%d", "Failed to close file descriptor ", fds[i][j]);
            }
        }
    }
}