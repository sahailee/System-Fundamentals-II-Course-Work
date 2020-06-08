#include <stdlib.h>
#include <unistd.h>
#include "debug.h"
#include "polya.h"
#include "worker_helper.h"

/*
 * worker
 * (See polya.h for specification.)
 */

static volatile sig_atomic_t got_cont = 0;
static volatile sig_atomic_t got_stop = 0;
static volatile sig_atomic_t got_cncl = 0;
static volatile sig_atomic_t got_term = 0;

/*
When a worker process receives a problem from the master process, it begins trying
to solve that problem.  It continues trying to solve the problem until one of the
following things happens: (1) A solution is found; (2) The solution procedure fails;
or (3) The master process notifies the worker to cancel the solution procedure.
In each case, the worker process sends back to the master process a result,
which indicates what happened and possibly contains a solution to the problem.
*/
/*
The actual reading and writing of data on the pipes can be done either using the low-level
"Unix I/O" read(2) and write(2) system calls, or else (probably somewhat more conveniently)
using the standard I/O library, after using fdopen(3) to wrap the pipe file descriptors
in FILE objects

When a worker process wants to read a problem sent by the master, it first reads
the fixed-size problem header, and continues by reading the problem data, the number of
bytes of which can be calculated by subtracting the size of the header from the total
size given in the size field of the header.
*/


int worker(void) {
    if(signal(SIGCONT, sig_cont_handle) == SIG_ERR) {
        debug("%s", "Error in signal function when creating signal handlers.");
        exit(EXIT_FAILURE);
    }
    signal(SIGSTOP, sig_cont_handle); //Interesting, if i do the same if statements, this if will execute.
    //I read the man pages, I believe its because u cant override how it handles sigstop.
    if(signal(SIGTERM, sig_cont_handle) == SIG_ERR) {
        debug("%s", "Error in signal function when creating signal handlers.");
        exit(EXIT_FAILURE);
    }
    if(signal(SIGHUP, sig_cont_handle) == SIG_ERR) {
        debug("%s", "Error in signal function when creating signal handlers.");
        exit(EXIT_FAILURE);
    }
    //debug("%s", "Worker is about to sssennd sigchld.");
    //kill(getppid(), SIGCHLD);
    //debug("%s", "Worker will send sigstop to itself.");
    kill(getpid(), SIGSTOP);
    while(1){
        //pause();
        if(got_stop) {
            got_stop = 0;
            pause();
        }
        if (got_term) {
            exit(EXIT_SUCCESS);
        }
        if(got_cont) {
            got_cont = 0;
            struct problem *prob = (struct problem *)read_problem();
            if (got_term) {
                free(prob);
                exit(EXIT_SUCCESS);
            }
            struct result *r = solvers[prob->type].solve(prob, &got_cncl);
            if(r != NULL && solvers[prob->type].check(r, prob) == 0){
                write_result(r);
                free(prob);
                free(r);
                got_cncl = 0;
                if(got_term) {
                    exit(EXIT_SUCCESS);
                }
                kill(getpid(), SIGSTOP);
            }
            else {
                //We have failed.
                if(r != NULL) {
                    free(r);
                }
                free(prob);
                if (got_term) {
                    exit(EXIT_SUCCESS);
                }
                write_fail();
                got_cncl = 0;
                kill(getpid(), SIGSTOP);
            }

        }
        else {
            debug("%s", "NO SIGCONT");
            got_stop = 1; //To make it go back to sleep and wait for CONT signal
        }
    }
    return EXIT_FAILURE;
}


void sig_cont_handle(int sig) {
    if(sig == SIGCONT) {
        got_cont = 1;
    }
    else if(sig == SIGSTOP) {
        //pause();
        got_stop = 1;
    }
    else if(sig == SIGTERM) {
        //We are donzo.
        //exit(EXIT_SUCCESS);
        got_term = 1;
        got_cncl = 1;
    }
    else if(sig == SIGHUP) {
        //Cancellation
        got_cncl = 1;
    }
}

void *read_problem() {
    //debug("%s", "Read problem from worker.");
    char *prob = malloc(sizeof(struct problem));
    if(prob == NULL) {
        debug("%s", "malloc returned NULL.");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < sizeof(struct problem); i++) {
        char c = fgetc(stdin);
        *(prob + i) = c;
    }
    size_t size = ((struct problem *) prob)->size;
    debug("%ld", size);
    prob = realloc(prob, size);
    if(prob == NULL) {
        debug("%s", "realloc returned NULL.");
        exit(EXIT_FAILURE);
    }
    char *prob_data = ((struct problem *)prob)->data;
    size -= sizeof(struct problem);
    for(int i = 0; i < size; i++) {
        prob_data[i] = fgetc(stdin);
    }
    //fflush(stdin);
    return prob;
}

void write_result(struct result *r) {
    for(int i = 0; i < r->size; i++) {
        fputc(((char *)r)[i], stdout);
    }
    fflush(stdout);
}
void write_fail() {
    struct result *r = malloc(sizeof(struct result));
    r->size = sizeof(struct result);
    r->failed = 1;
    for(int i = 0; i < r->size; i++) {
        fputc(((char *)r)[i], stdout);
    }
    fflush(stdout);
    free(r);
}