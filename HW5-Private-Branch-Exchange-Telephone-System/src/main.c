#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "pbx.h"
#include "server.h"
#include "debug.h"

#include "socket_helper_functions.h"


/*
 * "PBX" telephone exchange simulation.
 *
 * Usage: pbx <port>
 */
static void terminate(int status);
static void sig_handler(int sig) {
    if(sig == SIGHUP) {
        terminate(EXIT_SUCCESS);
    }
}

int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    if(argc <= 1) {
        debug("%s", "-p <port> argument required.");
        abort();
    }

    char c;
    char *port;
    int listenfd, *connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    while ((c = getopt (argc, argv, "p:")) != -1)
    switch (c)
      {
      case 'p':
        port = optarg;
        debug("%s", port);
        while(*optarg != '\0') {
            if(*optarg < '0' || *optarg > '9') {
                debug("%s", "Invalid port number.");
                abort();
            }
            optarg++;
        }
        break;
      default:
        debug("%s", "Invalid arguments. -p <port> is the only available option.");
        abort ();
      }

    // Perform required initialization of the PBX module.
    debug("Initializing PBX...");

    debug("Port number: %s", port);

    pbx = pbx_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function pbx_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server. This is not working.
    struct sigaction new_act;
    new_act.sa_flags = 0;
    sigemptyset(&new_act.sa_mask);
    new_act.sa_handler = sig_handler;
    if(sigaction(SIGHUP, &new_act, NULL) == -1) {
        debug("%s", "Error when installing signal.");
        abort();
    }

    listenfd = open_listenfd(port); //Should edit port num to become a char*.
    if(listenfd == -1){
        fprintf(stderr, "%s\n", "Can not create a server with given port number.");
        terminate(EXIT_FAILURE);
        exit(EXIT_FAILURE);
    }
    debug("File descriptor: %d", listenfd);
    debug("%s%d", "server_pid: ", getpid());

    while(1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = malloc(sizeof(int));
        *connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);
        pthread_create(&tid, NULL, pbx_client_service, connfd);
    }

    fprintf(stderr, "You have to finish implementing main() "
	    "before the PBX server will function.\n");

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
static void terminate(int status) {
    debug("Shutting down PBX...");
    pbx_shutdown(pbx);
    debug("PBX server terminating");
    exit(status);
}

