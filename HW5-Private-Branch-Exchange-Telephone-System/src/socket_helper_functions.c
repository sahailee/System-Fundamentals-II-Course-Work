#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "pbx.h"
#include "debug.h"
#include "socket_helper_functions.h"


/* Returns a listening descriptor that is ready to receive connection requests on the port.
* Code is from the textbook.
*/
int open_listenfd(char *port) {
    struct addrinfo hints, *listp, *p;
    int listenfd, optval = 1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_flags |= AI_NUMERICSERV;
    getaddrinfo(NULL, port, &hints, &listp); //Converts string representations of host names, port nums
    //and converts into a socket address structures

    for(p = listp; p; p = p->ai_next) {
        //* Create a socket descriptor */
        if((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            continue; //Socket failed. Try next.
        }

        //Elimate the "address already in use" error in bind
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
            (const void *)&optval, sizeof(int));

        // Bind the descriptor to the address
        if(bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
            break; //Success
        }

        close(listenfd);

    }
    freeaddrinfo(listp);
    if(!p) { //No addresses worked
        return -1;
    }
    if(listen(listenfd, PBX_MAX_EXTENSIONS) < 0) {
        close(listenfd);
        return -1;
    }
    return listenfd;
}


int Select(int  n, fd_set *readfds, fd_set *writefds,
       fd_set *exceptfds, struct timeval *timeout)
{
    int rc;

    if ((rc = select(n, readfds, writefds, exceptfds, timeout)) < 0)
        debug("Select error");
    return rc;
}

void Sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (sem_init(sem, pshared, value) < 0) {
        printf("Sem_init error");
        exit(0);
    }
}

void P(sem_t *sem)
{
    if (sem_wait(sem) < 0) {
        printf("P error");
        exit(0);
    }
}

void V(sem_t *sem)
{
    if (sem_post(sem) < 0) {
        printf("V error");
        exit(0);
    }
}