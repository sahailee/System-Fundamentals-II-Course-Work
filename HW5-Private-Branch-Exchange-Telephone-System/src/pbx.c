#include <stdlib.h>
#include <stdarg.h>
#include <sys/socket.h>
#include "pbx.h"
#include "debug.h"

#include "socket_helper_functions.h"

// The PBX system maintains the set of registered clients in the form of a mapping
// from assigned extension numbers to clients.
// It also maintains, for each registered client, information about the current
// state of the TU for that client.  The following are the possible states of a TU:


//IN MY MAIN.C TEST TRYING TO OPEN TWO PBX ON THE SAME PORT.
//ALSO TEST IF WE RUN OUT OF EXTENSIONS
//TODO: Check if the buddy disconnects so we can hangup
//Maybe I need to check feof in the server.c instead of what i do
int get_next_available_extension(PBX *pbx);
int send_notification(int fd, char *msg);
int send_notifications(int fd, int argc, ...);
char *int_to_string(int x);

struct tu {
    int fd;
    int ext;
    TU *buddy;
    PBX *linked_pbx;
    TU_STATE current_state;
    sem_t mutex;
};

struct pbx {
    int tup;
    int exiting;
    sem_t mutex;
    sem_t exit_sems[PBX_MAX_EXTENSIONS];
    TU *tus[PBX_MAX_EXTENSIONS];
};

PBX *pbx_init() {
    debug("%s%d", "pbx_init, max extensions: ", PBX_MAX_EXTENSIONS);
    PBX *n_pbx  = malloc(sizeof(struct pbx));
    if(n_pbx == NULL)
        return NULL;
    n_pbx->tup = 0;
    n_pbx->exiting = 0;
    Sem_init(&n_pbx->mutex, 0, 1);
    for(int i = 0; i < PBX_MAX_EXTENSIONS; i++) {
        n_pbx->tus[i] = NULL;
        Sem_init(&n_pbx->exit_sems[i], 0, 1);
    }
    return n_pbx;
}

void pbx_shutdown(PBX *pbx) {
    debug("%s", "Shutting down.");
    P(&pbx->mutex);
    pbx->exiting = 1;
    V(&pbx->mutex);
    for(int i = 0; i < PBX_MAX_EXTENSIONS; i++) {
        P(&pbx->mutex);
        if(pbx->tus[i] == NULL) {
            V(&pbx->mutex);
            continue;
        }
        V(&pbx->mutex);
        TU *tu = pbx->tus[i];
        shutdown(tu->fd, SHUT_RDWR);
        debug("Seding shutdown to: %d", i);
    }
    //I need to P something and have unregister V it and then try to V it. Ill have an array of sems.
    for(int i = 0; i < PBX_MAX_EXTENSIONS; i++) {
        P(&pbx->exit_sems[i]); //If it is unregistered it should be free.
        if(i < 10)
            debug("Confirmed shutdown: %d", i);
    }
    free(pbx);
    debug("%s", "Shut down complete.");
}

TU *pbx_register(PBX *pbx, int fd) {
    debug("%s", "tu_register");
    P(&pbx->mutex);
    int n = get_next_available_extension(pbx);
    if(n == -1) {
        V(&pbx->mutex);
        send_notification(fd, "No more available extensions. Please try again later.");
        return NULL; //No more available extensions
    }
    TU *n_tu = malloc(sizeof(struct tu));
    if(n_tu == NULL) {
        V(&pbx->mutex);
        return NULL;
    }
    //Setup this tu
    n_tu->fd = fd;
    n_tu->ext = n;
    n_tu->current_state = TU_ON_HOOK;
    n_tu->buddy = NULL;
    n_tu->linked_pbx = pbx;
    Sem_init(&n_tu->mutex, 0, 1);
    //Place tu in the pbx list. And move our pointer to the next tu spot.
    pbx->tus[n] = n_tu;
    pbx->tup = (pbx->tup + 1) % PBX_MAX_EXTENSIONS;
    P(&pbx->exit_sems[n]);
    //send a notification
    char *extension = int_to_string(n);
    debug("Extension is: %s", extension);
    debug("%s", "About to send notification.");
    if(send_notifications(n_tu->fd, 2, tu_state_names[n_tu->current_state], extension) < 0) {
        free(extension);
        V(&pbx->mutex);
        return NULL; //Need tp add extension number.
    }
    free(extension);
    V(&pbx->mutex);
    return n_tu;

}

int pbx_unregister(PBX *pbx, TU *tu) {
    if(tu == NULL)
        return -1;
    P(&pbx->mutex);
    P(&tu->mutex); //locks tu forever and then it is freed
    if(tu->buddy != NULL)
        P(&tu->buddy->mutex);
    if(tu->current_state == TU_CONNECTED || tu->current_state == TU_RINGING) {
        //P(&tu->buddy->mutex);
        tu->buddy->current_state = TU_DIAL_TONE;
        if(pbx->exiting == 0 && send_notification(tu->buddy->fd, tu_state_names[TU_DIAL_TONE]) < 0) {
            debug("%s", "Could not send notification to buddy about their new state.");
        }
        tu->buddy->buddy = NULL; //disassociate the two tus
        //V(&tu->buddy->mutex);
    }
    else if(tu->current_state == TU_RING_BACK) {
        //P(&tu->buddy->mutex);
        tu->buddy->current_state = TU_ON_HOOK;
        char *extension = int_to_string(tu->buddy->ext);
        if(pbx->exiting == 0 && send_notifications(tu->buddy->fd, 2, tu_state_names[TU_ON_HOOK], extension) < 0) {
            debug("%s", "Could not send notification to buddy about their new state.");
        }
        tu->buddy->buddy = NULL; //disassociate the two tus
        //V(&tu->buddy->mutex);
        free(extension);

    }
    debug("%s", "tu_unregister");
    if(pbx->tus[tu->ext] != tu) {
        debug("%s", "TU not register with out pbx. Freeing anyway. Returning error");
        if(tu->buddy != NULL)
            V(&tu->buddy->mutex);
        free(tu);
        V(&pbx->mutex);
        return -1;
    }
    pbx->tus[tu->ext] = NULL;
    if(tu->buddy != NULL)
        V(&tu->buddy->mutex);
    V(&pbx->mutex);
    int i = tu->ext;
    free(tu);
    V(&pbx->exit_sems[i]);
    return 0;

}

int tu_fileno(TU *tu) {
    debug("%s", "tu_fileno");
    if(tu == NULL || tu->fd < 0)
        return -1;
    return tu->fd;
}

int tu_extension(TU *tu) {
    debug("%s", "tu_extension");
    if(tu == NULL || tu->ext < 0)
        return -1;
    return tu->ext;
}

int tu_pickup(TU *tu) {
    P(&tu->mutex);
    if(tu->buddy != NULL)
        P(&tu->buddy->mutex);
    debug("%s", "tu_pickup");
    int success = 0;
    if(tu->current_state == TU_ON_HOOK) {
        tu->current_state = TU_DIAL_TONE;
        success = send_notification(tu->fd, tu_state_names[TU_DIAL_TONE]);

    }
    else if(tu->current_state == TU_RINGING) {
        //P(&tu->buddy->mutex);
        if(tu->buddy->current_state != TU_RING_BACK)
            debug("\n%s\n", "ERR: BUDDY NOT IN RING BACK STATE.");
        tu->current_state = TU_CONNECTED;
        tu->buddy->current_state = TU_CONNECTED;
        //send notifications
        char *extension = int_to_string(tu->buddy->ext);
        if(send_notifications(tu->fd, 2, tu_state_names[TU_CONNECTED], extension) < 0)
            success = -1;
        free(extension);
        extension = int_to_string(tu->ext);
        if(send_notifications(tu->buddy->fd, 2, tu_state_names[TU_CONNECTED], extension) < 0)
            success = -1;
        free(extension);
    }
    else {
        success = send_notification(tu->fd, tu_state_names[tu->current_state]);
    }
    if(tu->buddy != NULL)
        V(&tu->buddy->mutex);
    V(&tu->mutex);
    return success;
}

int tu_hangup(TU *tu) {
    P(&tu->mutex);
    if(tu->buddy != NULL)
        P(&tu->buddy->mutex);
    debug("%s", "tu_hangup");
    int success = 0;
    if(tu->current_state == TU_CONNECTED) {
        //P(&tu->buddy->mutex);
        tu->current_state = TU_ON_HOOK;
        tu->buddy->current_state = TU_DIAL_TONE;
        char *extensions = int_to_string(tu->ext);
        if(send_notifications(tu->fd, 2, tu_state_names[TU_ON_HOOK], extensions) < 0)
            success = -1;
        free(extensions);
        if(send_notification(tu->buddy->fd, tu_state_names[TU_DIAL_TONE]) < 0)
            success = -1;
        tu->buddy->buddy = NULL; //disassociate the two tus
        V(&tu->buddy->mutex);
        tu->buddy = NULL;
    }
    else if(tu->current_state == TU_RING_BACK) {
        //P(&tu->buddy->mutex);
        if(tu->buddy->current_state != TU_RINGING)
            debug("\n%s\n", "ERR: BUDDY NOT IN RINGING STATE.");
        tu->current_state = TU_ON_HOOK;
        tu->buddy->current_state = TU_ON_HOOK;
        char *extensions = int_to_string(tu->ext);
        if(send_notifications(tu->fd, 2, tu_state_names[TU_ON_HOOK], extensions) < 0)
            success = -1;
        free(extensions);
        extensions = int_to_string(tu->buddy->ext);
        if(send_notifications(tu->buddy->fd, 2, tu_state_names[TU_ON_HOOK], extensions) < 0)
            success = -1;
        free(extensions);
        tu->buddy->buddy = NULL; //disassociate the two tus
        V(&tu->buddy->mutex);
        tu->buddy = NULL;
    }
    else if(tu->current_state == TU_RINGING){
        //P(&tu->buddy->mutex);
        if(tu->buddy->current_state != TU_RING_BACK)
            debug("\n%s\n", "ERR: BUDDY NOT IN RING BACK STATE.");
        tu->current_state = TU_ON_HOOK;
        tu->buddy->current_state = TU_DIAL_TONE;
        char *extension = int_to_string(tu->ext);
        if(send_notifications(tu->fd, 2, tu_state_names[TU_ON_HOOK], extension) < 0)
            success = -1;
        free(extension);
        if(send_notification(tu->buddy->fd, tu_state_names[TU_DIAL_TONE]) < 0)
            success = -1;
        tu->buddy->buddy = NULL; //disassociate the two tus
        V(&tu->buddy->mutex);
        tu->buddy = NULL;
    }
    else if(tu->current_state == TU_DIAL_TONE || tu->current_state == TU_BUSY_SIGNAL
        || tu->current_state == TU_ERROR || tu->current_state == TU_ON_HOOK) {
        tu->current_state = TU_ON_HOOK;
        char *extension = int_to_string(tu->ext);
        success = send_notifications(tu->fd, 2, tu_state_names[TU_ON_HOOK], extension);
        free(extension);
    }
    else {
        success = send_notification(tu->fd, tu_state_names[tu->current_state]);
    }
    if(tu->buddy != NULL)
        V(&tu->buddy->mutex);
    V(&tu->mutex);
    return success;

}

int tu_dial(TU *tu, int ext) {
    P(&tu->mutex);
    P(&tu->linked_pbx->mutex);
    debug("%s", "tu_dial");
    int success = 0;
    if(tu->current_state == TU_ON_HOOK) {
            char *extensions = int_to_string(tu->ext);
            if(send_notifications(tu->fd, 2, tu_state_names[TU_ON_HOOK], extensions) < 0)
                success = -1;
            free(extensions);
        }
    else if(tu->linked_pbx->tus[ext] == NULL) {
        tu->current_state = TU_ERROR;
        success = send_notification(tu->fd, tu_state_names[TU_ERROR]);
    }
    else {
        if(tu->current_state == TU_DIAL_TONE && ext == tu->ext) {
            tu->current_state = TU_BUSY_SIGNAL;
            success = send_notification(tu->fd, tu_state_names[tu->current_state]);
        }
        else if(tu->current_state == TU_DIAL_TONE) {
            P(&tu->linked_pbx->tus[ext]->mutex);
            if(tu->linked_pbx->tus[ext]->current_state == TU_ON_HOOK) {
                tu->current_state = TU_RING_BACK;
                tu->linked_pbx->tus[ext]->current_state = TU_RINGING;
                //Set the tus to be buddys. This should matter if the person picksup
                tu->buddy = tu->linked_pbx->tus[ext];
                tu->linked_pbx->tus[ext]->buddy = tu;
                //send notifications
                if(send_notification(tu->fd, tu_state_names[TU_RING_BACK]) < 0)
                    success = -1;
                if(send_notification(tu->buddy->fd, tu_state_names[TU_RINGING]) < 0)
                    success = -1;
            }
            else {
                tu->current_state = TU_BUSY_SIGNAL;
                success = send_notification(tu->fd, tu_state_names[tu->current_state]);
            }
            V(&tu->linked_pbx->tus[ext]->mutex);

        }
        else {
            success = send_notification(tu->fd, tu_state_names[tu->current_state]);
        }
    }
    V(&tu->linked_pbx->mutex);
    V(&tu->mutex);
    return success;

}

int tu_chat(TU *tu, char *msg) {
    P(&tu->mutex);
    debug("%s", "tu_chat");
    int success = 0;
    if(tu->current_state != TU_CONNECTED) {
        success = send_notification(tu->fd, tu_state_names[tu->current_state]);
        V(&tu->mutex);
        return success;
    }
    else {
        //send message and return 0 if successful.
        //The tu sends to the buddy.
        P(&tu->buddy->mutex);
        char *extension = int_to_string(tu->buddy->ext);
        if(send_notifications(tu->fd, 2, tu_state_names[tu->current_state], extension) < 0)
            success = -1;
        free(extension);
        if (send_notifications(tu->buddy->fd, 2, "CHAT", msg) < 0)
            success = -1;
        V(&tu->buddy->mutex);
        V(&tu->mutex);
        return success;
    }

}


/*
* Returns the first available extension number.
* Returns -1 if all full.
*/
int get_next_available_extension(PBX *pbx) {
    debug("%s", "get_next_available_extension");
    int i = pbx->tup;
    do {
        if(pbx->tus[i] == NULL) {
            debug("%s%d", "Found extension num: ", i);
            return i;
        }
        i = (i + 1) % PBX_MAX_EXTENSIONS;

    } while(i != pbx->tup);
    debug("%s", "Could not find extension.");
    return -1;
}

/*
* Writes the message to the fd.
* Returns -1 on error and 0 on success.
*/
int send_notification(int fd, char *msg) {
    debug("%s", "Send Notification Method");
    debug("%s", msg);
    FILE *fp = fdopen(fd, "w");
    if(fputs(msg, fp) == EOF) {
        debug("%s", "Error in writing to file.");
        return -1;
    }
    fputc('\r', fp);
    fputc('\n', fp);
    fflush(fp);
    //fclose(fp);
    return 0;
}

int send_notifications(int fd, int argc, ...) {
    va_list msgs;
    va_start(msgs, argc);
    FILE *fp = fdopen(fd, "w");
    debug("%s", "Opened the fd");
    for(int i = 0; i < argc; i++) {
        if(fputs(va_arg(msgs, char *), fp) == EOF) {
            debug("%s", "Error in writing to file.");
            return -1;
        }
        fputc(' ', fp);
        debug("%s", "Wrote one string.");
    }
    fputc('\r', fp);
    fputc('\n', fp);
    va_end(msgs);
    fflush(fp);
    //fclose(fp);
    debug("%s", "Returning from method.");
    return 0;
}

char *int_to_string(int x) {
    if(x == 0) {
        char *result = malloc(2);
        *result = '0';
        result[1] = '\0';
        return result;
    }
    int a = x;
    int length = 0;
    while(a != 0) {
        a /= 10;
        length++;
    }
    char *result = malloc(length + 1);
    debug("Num bytes: %d", length);
    for(int i = length - 1; i >= 0; i--) {
        result[i] = (x % 10) + '0';
        x /= 10;
    }
    result[length] = '\0';
    return result;
}
