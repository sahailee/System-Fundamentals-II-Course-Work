#include <unistd.h>

void init_workers(int workers, int pipefds[][2]);

void sig_master_handler(int sig);

void write_problem(int fd, void *prob);

void *read_result(int fd);

int get_index_for_pid(pid_t cid, int size, pid_t *cpids);

int assign_prob_to_worker(int i, int workers, int fds[workers][2]);

int is_all_idle(int workers);

// struct read_write_fd {
//     int read;
//     int write;
// };

void close_all_fds(int workers, int fds[workers][2]);