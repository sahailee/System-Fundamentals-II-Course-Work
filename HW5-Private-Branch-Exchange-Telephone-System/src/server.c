#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#include "pbx.h"
#include "server.h"
#include "debug.h"

#include "socket_helper_functions.h"

//I use these macros to determine which TU function to call.
#define PCKUP 1
#define HNGUP 2
#define DIAL 3
#define CHAT 4

int found_EOF = 0;

static void pipe_handler(int status) {
    //do nothing
}

/*
 * Thread function for the thread that handles a particular client.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This variable must be freed once the file
 * descriptor has been retrieved.
 * @return  NULL
 *
 * This function executes a "service loop" that receives messages from
 * the client and dispatches to appropriate functions to carry out
 * the client's requests.  The service loop ends when the network connection
 * shuts down and EOF is seen.  This could occur either as a result of the
 * client explicitly closing the connection, a timeout in the network causing
 * the connection to be closed, or the main thread of the server shutting
 * down the connection as part of graceful termination.
 */
void *pbx_client_service(void *arg) {
    int connfd = *((int *) arg);
    pthread_detach(pthread_self());
    free((int *)arg);
    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = pipe_handler;
    if(sigaction(SIGPIPE, &sa, NULL) == -1) {
        debug("%s", "Error when installing sigpipe signal");
        //abort();
    }
    //PBX *this_pbx = pbx_init();
    TU *this_tu = pbx_register(pbx, connfd);
    FILE *fp = fdopen(connfd, "r");
    if(this_tu == NULL) {
        debug("%s", "TU IS NULL. should only happen if no more extensions left");
        //fprintf(stderr, "%s", "Sorry, no more extensions available. Try again later.");
        found_EOF = 1;
    }
    debug("%s", "Opening TU");
    //fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL) | O_NONBLOCK); //Set the fd to not block
    // fd_set read_set, ready_set;
    // FD_ZERO(&read_set);
    // FD_SET(connfd, &read_set);
    while(!found_EOF) { //Need to read the command
        //If the first character is a p or an h then the length of the command is 6.
        // ready_set = read_set;
        // Select(connfd+1, &ready_set, NULL, NULL, NULL);
        //if(FD_ISSET(connfd, &ready_set)) {
            int x = read_command(fp);
            debug("%s", "Read Command");
            if(x == PCKUP) {
                tu_pickup(this_tu);
            }
            else if(x == HNGUP) {
                tu_hangup(this_tu);
            }
            else if(x == DIAL) {
                int ext = read_extension(fp); // I think i have an error because I have the file open
                //I probably need to close it. Whoever is recieving should have the file closed.
                tu_dial(this_tu, ext);
            }
            else if(x == CHAT) {
                int n;
                char *msg = read_message(fp, &n);
                if(msg == NULL) {
                    break;
                }
                int i = 0;
                for(; i < n; i++) { //Trimming the beginning to eliminate extra spaces
                    if(msg[i] != ' ') {
                        break;
                    }
                }
                debug("Original message of size %d: %s", n, msg);
                if(tu_chat(this_tu, (msg + i)) == 0) {
                    free(msg);
                }
                else {
                    free(msg);
                }
                //free(msg);
            }
        //}

    }
    debug("%s", "Closing TU");
    found_EOF = 0;
    pbx_unregister(pbx, this_tu);
    debug("%s", "Unregistered.");
    close(connfd);
    //pbx_shutdown(this_pbx); //Maybe need this idk
    return NULL;
}

/*
* Read the first word in the sequence and determine if it is a valid command.
* Its return value let's me know which command it was able to parse.
* Returning 0 means it matched to nothing. -1 means EOF
*/
int read_command(FILE *fp) {
    debug("%s", "Start of read command method, no bytes read yet.");
    char cmd = fgetc(fp);
    int x;
    if(cmd == 'p') {
        if((x = matchtocommand(fp, "pickup\r\n", 8)) > 0) {
            return PCKUP;
        }
        else if(x < 0) {
            return -1;
        }

    }
    else if(cmd == 'h') {
        if((x = matchtocommand(fp, "hangup\r\n", 8)) > 0) {
            return HNGUP;
        }
        else if(x < 0) {
            return -1;
        }
    }
    else if(cmd == 'd') {
        if((x = matchtocommand(fp, "dial ", 5)) > 0) {
            return DIAL;
        }
        else if(x < 0) {
            return -1;
        }
    }
    else if(cmd == 'c') {
        if((x = matchtocommand(fp, "chat", 4)) > 0) {
            return CHAT;
        }
        else if(x < 0) {
            return -1;
        }
    }
    else if(cmd == EOF) {
        debug("%s", "Found EOF in read_command");
        found_EOF = 1;
        return -1;
    }
    return 0;
}

/*
* This is going to read the first word from fd and check if it macthes the command argument.
* Returns 0 if false, 1 if they match, -1 if found EOF
*/
int matchtocommand(FILE *fp, const char *command, int length) {
    char c;
    for(int i = 1; i < length; i++) {
        c = fgetc(fp);
        if(c == EOF) {
            debug("%s", "Found EOF in matchtocommand");
            found_EOF = 1;
            return -1;
        }
        if(c != command[i]) { //Now we read until we get the EOL characters.
            debug("%s", "Invalid command.");
            return read_rest_of_line(fp, c);
        }
    }
    debug("%s", "Succussfully matched command");
    return 1;
}

int read_extension(FILE *fp) {
    int ext = 0;
    char c;
    while((c = fgetc(fp))) {
        if(c == EOF) {
            debug("%s", "Found EOF when looking for extension.");
            found_EOF = 1;
            return -1;
        }
        if(c == '\r') {
            c = fgetc(fp);
            if(c == '\n') {
                break;
            }
            else {
                debug("%s", "invalid characters for extension.");
                return read_rest_of_line(fp, c);
            }
        }
        if(c < '0' || c > '9') {
            debug("%s", "invalid characters for extension.");
            return read_rest_of_line(fp, c);
        }
        ext = (ext * 10) + (c - '0');

    }
    debug("Returning extension: %d", ext);
    return ext;
}

/*
* Read the rest of the sequence just to discard it
* c is the current character read.
*/
int read_rest_of_line(FILE *fp, char c) {
    debug("%s", "reading rest of line.");
    while(c != EOF) {
        while(c != EOF && c != '\r') {
            c = fgetc(fp);
        }
        if(c == EOF)
            break;
        c = fgetc(fp);
        if(c == '\n') {
            debug("%s", "Succussfully found EOL sequence");
            return 0;
        }
    }
    debug("%s", "Found EOF when reading rest of line.");
    found_EOF = 1;
    return -1;
}

char *read_message(FILE *fp, int *length) {
    char c;
    int size = 20;
    int i = 0;
    char *msg = malloc(size);
    int foundr = 0;
    int foundn = 0;
    while(!foundr || !foundn) {
        c = fgetc(fp);
        if(c == EOF) {
            free(msg);
            found_EOF = 1;
            return NULL;
        }
        if(c == '\r') {
            foundr = 1;
        }
        else if(foundr && (c == '\n')) {
            foundn = 1;
        }
        else {
            foundr = 0;
            foundn = 0;
            msg[i] = c;
            i++;
            if(i >= size) {
                size *= 2;
                msg = realloc(msg, size);
            }
        }
    }
    msg[i] = '\0';
    i++;
    msg = realloc(msg, i); //Free up any extra space we have.
    //msg[i] = '\0';
    //debug("%c%c", msg[i - 2], msg[i - 1]);
    *length = i;
    debug("Number of bytes of message is %d", i);
    return msg;
}

/*
*Read using read only one byte.
*
*/
char readbyte(int fd) {
    char c;
    int x;
    if((x = read(fd, &c, 1)) < 0) {
        return 0;
    }
    else if(x == 0) {
        return EOF;
    }

    return c;
}
