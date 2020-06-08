#include <semaphore.h>

int open_listenfd(char *port);
int Select(int  n, fd_set *readfds, fd_set *writefds,
       fd_set *exceptfds, struct timeval *timeout);
int matchtocommand(FILE *fp, const char *command, int length);
int read_command(FILE *fp);
int read_rest_of_line(FILE *fp, char c);
int read_extension(FILE *fp);
char *read_message(FILE *fp, int *length);
char readbyte(int fd);
void Sem_init(sem_t *sem, int pshared, unsigned int value);
void P(sem_t *sem);
void V(sem_t *sem);