#include <stdlib.h>
//#include <sys/types.h>
#include <sys/wait.h>
#include "debug.h"
#include "polya.h"
#include "master_helper.h"

/*
 * master
 * (See polya.h for specification.)
 */

// static volatile sig_atomic_t pid;
// static volatile sig_atomic_t wstatus;
static volatile sig_atomic_t sigpipe_flag;
static volatile sig_atomic_t *sigchld_flag;


static volatile pid_t *children;
static volatile sig_atomic_t tot_workers;

static volatile sig_atomic_t *worker_states;
static struct problem **current_problems;

//static pid_t current_pid;

static volatile sig_atomic_t signal_worker_count;
//static struct read_write_fds *masterfds;
//static struct read_write_fds *childfds;

//TODO: Go back to worker and check the return values of malloc, free and fputc etc for errors
//Signals left to Handle: SIGPIPE
//Need to figure out when to cancel a solution.
int master(int workers) {
   // current_pid = getpid();
    signal_worker_count = 0;

    children = malloc(sizeof(pid_t) * workers);
    int fds[workers][2];
    //master_helper = malloc(sizeof(struct))

    worker_states = malloc(sizeof(volatile sig_atomic_t) * workers);
    sigchld_flag = malloc(sizeof(volatile sig_atomic_t) * workers);

    int num_workers_with_no_probs_left = 0;

    //int workers_idle = 0;
    int no_more_problems[workers];

    current_problems = malloc(sizeof(struct problem *) * workers);

    tot_workers = workers;
    sf_start(); //Must Call
    sigset_t mask, prev;
    if(signal(SIGCHLD, sig_master_handler) == SIG_ERR) {
        debug("%s", "Error in signal function when creating signal handlers.");
        exit(EXIT_FAILURE);
    }
    if(signal(SIGPIPE, sig_master_handler) == SIG_ERR) {
        debug("%s", "Error in signal function when creating signal handlers.");
        exit(EXIT_FAILURE);
    }
    sigemptyset(&mask);
    // sigemptyset(&prev);
    sigaddset(&mask, SIGCHLD);
    //sigaddset(&mask, SIGPIPE);
    //I also need to block SIGPIPE most likely.
    sigprocmask(SIG_BLOCK, &mask, &prev); /* Block SIGCHLD */
    init_workers(workers, fds);
    sigsuspend(&prev);
    //sigprocmask(SIG_SETMASK, &prev, NULL); /* Unblock SIGCHLD */
    //Then I will have my my main loop and dish out the problems
    //Need to call get_problem_variant and post_result
    //return of waitpid tells us which worker it came frome
    //pause();
    //sigprocmask(SIG_BLOCK, &mask, &prev); /* Block SIGCHLD */
    //sigsuspend(&prev);
    while(1) {
        //debug("%s", "About to pause");
        //sigsuspend(&prev);
        //debug("%s", "Unpaused");
        //sigsuspend(&prev);
        //sigprocmask(SIG_BLOCK, &mask, &prev); /* Block SIGCHLD */
        num_workers_with_no_probs_left = 0;
        //debug("%s", "Master has recieved a signal.");
        if(sigpipe_flag == 1) {
            sigpipe_flag = 0;
            debug("%s", "Recieved sigpipe signal.");
            //continue;
        }
        //sigprocmask(SIG_BLOCK, &mask, &prev); /* Block SIGCHLD */
        for(int i = 0; i < workers; i++) {
            if(sigchld_flag[i] == 0) {
                continue;
            }
            sigchld_flag[i] = 0;
            if(worker_states[i] == WORKER_CONTINUED) {
                //debug("%d%s", children[i], " is now in the continued state");
            }
            else if(worker_states[i] == WORKER_STARTED) {
                //debug("%d%s", children[i], " is now in the started state");
                sf_change_state(children[i], WORKER_STARTED, WORKER_IDLE);
                worker_states[i] = WORKER_IDLE;
            }
            else if(worker_states[i] == WORKER_IDLE) {
                //debug("%d%s", children[i], " is now in the idle state");
                int x = assign_prob_to_worker(i, workers, fds);
                // num_workers_with_no_probs_left += x;
                if(x == 0) {
                    no_more_problems[i] = 0;
                    sf_change_state(children[i], worker_states[i], WORKER_CONTINUED);
                    worker_states[i] = WORKER_CONTINUED;
                    if(kill(children[i], SIGCONT) == -1) {//Send the signal
                        debug("%s", "Error in sending SIGCONT signal to child.");
                        exit(EXIT_FAILURE);
                    }
                }
                else {
                    no_more_problems[i] = 1;
                }
            }
            else if(worker_states[i] == WORKER_RUNNING) {
                //debug("%d%s", children[i], " is now in the running state");
            }
            else if(worker_states[i] == WORKER_STOPPED && current_problems[i] != NULL) {
               // debug("%d%s", children[i], " is now in the stopped state");
                struct result *res = read_result(fds[i][0]);
                sf_recv_result(children[i], res);
                struct problem *temp_prob = current_problems[i];
                short type = temp_prob->type;
                int crct = post_result(res, temp_prob);
                current_problems[i] = NULL;
                if(crct == 0) {
                    for(int j = 0; j < workers; j++) {
                        if(j == i) {
                            continue;
                        }
                        struct problem *jprob = current_problems[j];
                        if(jprob != NULL){
                            if(type == jprob->type) {
                                //debug("%s", "These problems are the same.");
                                sf_cancel(children[j]);
                                if(kill(children[j], SIGHUP) == -1) {//Send the signal
                                    debug("%s", "Error in sending SIGHUP signal to child.");
                                    exit(EXIT_FAILURE);
                                }
                                current_problems[j] = NULL;
                            }
                        }
                    }
                }
                free(res);
                sf_change_state(children[i], WORKER_STOPPED, WORKER_IDLE);

                worker_states[i] = WORKER_IDLE;
                //debug("%d%s", children[i], " is now in the idle state");
                int x = assign_prob_to_worker(i, workers, fds);
                // num_workers_with_no_probs_left += x;
                if(x == 0) {
                    no_more_problems[i] = 0;
                    sf_change_state(children[i], worker_states[i], WORKER_CONTINUED);
                    worker_states[i] = WORKER_CONTINUED;
                    if(kill(children[i], SIGCONT) == -1) {//Send the signal
                        debug("%s", "Error in sending SIGCONT signal to child.");
                        exit(EXIT_FAILURE);
                    }
                }
                else {
                    no_more_problems[i] = 1;
                }
            }
            else if(worker_states[i] == WORKER_STOPPED) {
                debug("%s", "Worker stopped and current prob is NULL");
                sf_change_state(children[i], WORKER_STOPPED, WORKER_IDLE);

                worker_states[i] = WORKER_IDLE;
                //debug("%d%s", children[i], " is now in the idle state");
                int x = assign_prob_to_worker(i, workers, fds);
                // num_workers_with_no_probs_left += x;
                if(x == 0) {
                    no_more_problems[i] = 0;
                    sf_change_state(children[i], worker_states[i], WORKER_CONTINUED);
                    worker_states[i] = WORKER_CONTINUED;
                    if(kill(children[i], SIGCONT) == -1) {//Send the signal
                        debug("%s", "Error in sending SIGCONT signal to child.");
                        exit(EXIT_FAILURE);
                    }
                }
                else {
                    no_more_problems[i] = 1;
                }
            }
            else if(worker_states[i] == WORKER_ABORTED) {
                debug("%d%s", children[i], " is now in the aborted state");
                exit(EXIT_FAILURE);
            }
            else if(worker_states[i] == WORKER_EXITED) {
                debug("%d%s%d", children[i], " is now in the exited state: ", worker_states[i]);
            }
            else {
                debug("%d%s%d", children[i], " is now in a unknown state: ", worker_states[i]);
            }
        }
        for(int i = 0; i < workers; i++) {
            if(no_more_problems[i] == 1) {
                num_workers_with_no_probs_left++;
            }
        }
        debug("%s%d", "num_workers_with_no_probs_left: ", num_workers_with_no_probs_left);
        if(num_workers_with_no_probs_left >= workers) {
            if(is_all_idle(workers)) {
                //sigprocmask(SIG_BLOCK, &mask, &prev);
                break;
            }
        }
        //pause();
        //debug("%s", "paused");
        sigsuspend(&prev);
        //debug("%s", "unpaused");

    }
    debug("%s", "Outside of main loop");
    for(int i = 0; i < workers; i++) {
        if(kill(children[i], SIGTERM) == -1) {
            debug("%s", "Error in sending SIGTERM signal to child.");
            exit(EXIT_FAILURE);
        }
        if(kill(children[i], SIGCONT) == -1) {//Send the signal
            debug("%s", "Error in sending SIGCONT signal to child.");
            exit(EXIT_FAILURE);
        }
    }
    sigsuspend(&prev);
    //sigprocmask(SIG_SETMASK, &prev, NULL); /* Unblock SIGCHLD */
    int all_exited = 0;
    while(all_exited < workers) {
        //sigsuspend(&prev);
        if(sigpipe_flag == 1) {
            debug("%s\n", "SIGPIPE flag is set to 1.");
        }
        all_exited = 0;
        for(int i = 0; i < workers; i++) {
            if(worker_states[i] == WORKER_EXITED) {
                //debug("%s%d%s", "Woker: ", children[i],  " confirmed exit.");
                all_exited++;
            }
            else if(worker_states[i] == WORKER_ABORTED) {
                //debug("%s%d%s", "Woker: ", children[i],  " confirmed aborted.");
                all_exited++;
            }
            else {
                //debug("%d%s%d%s", children[i], " is ", worker_states[i], " states");
            }
        }
        if(all_exited >= workers) {
            break;
        }
        else {
            debug("%s%d", "Tot workers exited: ", all_exited);
            sigsuspend(&prev);
            //pause();
        }
    }
    for(int i = 0; i < workers; i++) {
        if(worker_states[i] != WORKER_EXITED) {
            debug("%s", "Failue");
            sf_end();
            exit(EXIT_FAILURE);
        }
    }
    sigprocmask(SIG_SETMASK, &prev, NULL); /* Unblock SIGCHLD */
    close_all_fds(workers, fds);
    free((void *)children);
    free((void *)worker_states);
    free((void *)sigchld_flag);
    free(current_problems);
    sf_end(); //Must call
    exit(EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

void init_workers(int workers, int pipefds[workers][2]) {
    for(int i = 0; i < workers; i++) {
        if(pipe(pipefds[i]) == -1) {
            debug("%s", "Call to pipe returns error. Exiting...");
            exit(EXIT_FAILURE);
        }
        int worker_to_master[2];
        if(pipe(worker_to_master) == -1) {
            debug("%s", "Call to pipe returns error. Exiting...");
            exit(EXIT_FAILURE);
        }
        pid_t new_pid = fork();
        if(new_pid == -1) {
            debug("%s", "Call to fork returns error. Exiting...");
            exit(EXIT_FAILURE);
        }
        if(new_pid == 0) { // This is the child process
            //Also we may need to close pipes but I am not sure because I though
            //In our assignment both the processes read and write to the pipes.
            dup2(pipefds[i][0], STDIN_FILENO); //Set stdin to be the read pipe
            dup2(worker_to_master[1], STDOUT_FILENO); //Set stdouf to be the write pipe
            close(pipefds[i][1]);
            close(worker_to_master[0]);
            //debug("%s%d%s%d", "Pipe to write: ", worker_to_master[1], " Pipe to Read: ", pipefds[i][0]);
            if(execl("bin/polya_worker", "poly_worker", (char *) NULL) == -1) { //Maybe second arg is "polya_worker"
                debug("%s", "Failed to execute bin/polya_worker.");
                exit(EXIT_FAILURE); //Might need to send a signal to the parent process.
            }
            exit(EXIT_SUCCESS);

        }
        else { //parent process
            children[i] = new_pid;
            sf_change_state(children[i], 0, WORKER_STARTED);
            worker_states[i] = WORKER_STARTED;
            close(pipefds[i][0]);
            close(worker_to_master[1]);
            pipefds[i][0] = worker_to_master[0];
            //debug("%s%d%s%d", "Pipe to write: ", pipefds[i][1], " Pipe to Read: ", pipefds[i][0]);

        }

    }
}


void sig_master_handler(int sig) {
    if(sig == SIGCHLD) {
        int wstatus = 0;
        pid_t pid;
        while((pid = waitpid(-1, &wstatus, WUNTRACED | WCONTINUED | WNOHANG)) > 0) { //Prpblem is right here
            int i = -1;
            for(int j = 0; j < tot_workers; j++) {
                if(children[j] == pid) {
                    i = j;
                    break;
                }
            }
            if(i < 0 || i > tot_workers) {
                if(pid == -1){
                    return;
                }
                else {
                    continue;
                }
            }
            if(WIFEXITED(wstatus)) { //This child terminated successfully.
                if(WEXITSTATUS(wstatus) == EXIT_SUCCESS) { //Some State -> Exited
                    sf_change_state(pid, worker_states[i], WORKER_EXITED);
                    worker_states[i] = WORKER_EXITED;
                }
                else { //Worker terminated unsuccessfully Some State -> Aborted
                    sf_change_state(pid, worker_states[i], WORKER_ABORTED);
                    worker_states[i] = WORKER_ABORTED;
                }
            }
            else if(WIFSTOPPED(wstatus)) {
                if(worker_states[i] == WORKER_STARTED) { //started->idle
                    sf_change_state(pid, worker_states[i], WORKER_IDLE);
                    worker_states[i] = WORKER_IDLE;
                }
                else if(worker_states[i] == WORKER_IDLE) { //idle -> idle leave it be
                    //leave it as idle
                    sf_change_state(pid, WORKER_IDLE, WORKER_IDLE);
                    worker_states[i] = WORKER_IDLE;
                }
                else if(worker_states[i] == WORKER_STOPPED) {
                    sf_change_state(pid, worker_states[i], WORKER_IDLE); //stop -> idle
                    worker_states[i] = WORKER_IDLE;
                }
                else { //Some state -> Stop
                    sf_change_state(pid, worker_states[i], WORKER_STOPPED); //some state -> stop
                    worker_states[i] = WORKER_STOPPED;
                }

            }
            else if(WIFCONTINUED(wstatus)) { //some state -> Running
                sf_change_state(pid, worker_states[i], WORKER_RUNNING);
                worker_states[i] = WORKER_RUNNING;
                // if(worker_states[i] == WORKER_CONTINUED) {
                //     sf_change_state(pid, worker_states[i], WORKER_RUNNING);
                //     worker_states[i] = WORKER_RUNNING;
                // }
                // else {
                //     sf_change_state(pid, worker_states[i], WORKER_CONTINUED);
                //     worker_states[i] = WORKER_CONTINUED;
                // }

            }
            sigchld_flag[i] = 1;
        }
    }
    if(sig == SIGPIPE) {
        sigpipe_flag = 1;
    }

}

int is_all_idle(int workers) {
    for(int i = 0; i < workers; i++) {
        if(worker_states[i] != WORKER_IDLE) {
            return 0;
        }
    }
    return 1;
}


int assign_prob_to_worker(int i, int workers, int fds[workers][2]) {
    pid_t pid = children[i];
    struct problem *prob = get_problem_variant(workers, i); //This creates the problems
    current_problems[i] = prob;
    if(prob != NULL) {
        write_problem(fds[i][1], prob);
        //debug("%s", "Wrote problem");
        sf_send_problem(pid, prob);
        return 0;
    }
    return 1;
}