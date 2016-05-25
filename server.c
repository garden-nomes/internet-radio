#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#define MAX_CLIENTS 256
#define BUF_SIZE 1024
#define SAMPLE_SIZE 96000

void *sender_thread(void *);
void *listener_thread(void *);

char *filename;
int server_fd;
int done = 0;

/* linked list of clients */
struct client {
    int fd;
    struct client *next;
};

struct client *remove_client(struct client *c);
struct client *add_client(int fd);
void destroy_clients(void);

struct client *c_head = NULL;
pthread_mutex_t client_list_lock = PTHREAD_MUTEX_INITIALIZER;


int main(int argc, char *argv[]) {
    int port, error;
    pthread_t listener, sender;

    /* usage */

    if (argc < 3) {
        fprintf(stderr, "Usage: %s port filename\n", argv[0]);
        return 1;
    }
    port = atoi(argv[1]);
    filename = argv[2];

    /* start threads */

    if ((error = pthread_create(&listener, NULL, listener_thread, &port))
            || (error = pthread_create(&sender, NULL, sender_thread, NULL)))
        fprintf(stderr, "Failed to create thread: %s\n", strerror(error));

    if ((error = pthread_join(sender, NULL))
            || (error = pthread_join(listener, NULL)))
        fprintf(stderr, "Failed to join thread: %s\n", strerror(error));

    /* clean up */

    destroy_clients();
    close(server_fd);
    pthread_mutex_destroy(&client_list_lock);
    return 0;
}

void *listener_thread(void *data) {
    struct client *c;
    int server_fd, client_fd, len_cli_addr, port;
    struct sockaddr_in cli_addr, serv_addr;

    port = *((int *)data);

    /* initialize networking */

    signal(SIGPIPE, SIG_IGN);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        return NULL;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))
            == -1) {
        perror("Error on binding");
        return NULL;
    }

    if (listen(server_fd, 5) == -1) {
        perror("Error on listen");
        return NULL;
    }

    len_cli_addr = sizeof(cli_addr);

    while (!done) {

        /* accept client connection */

        client_fd = accept(server_fd, (struct sockaddr *)&cli_addr,
            (socklen_t *)&len_cli_addr);
        if (client_fd == -1) {
            perror("Error on accept");
            return NULL;
        }

        fprintf(stderr, "Client received\n");

        /* add to clients list */

        pthread_mutex_lock(&client_list_lock);
        add_client(client_fd);
        pthread_mutex_unlock(&client_list_lock);
    }

    return NULL;
}

void *sender_thread(void *data) {
    int file_fd, n;
    char *p_buffer;
    char buffer[BUF_SIZE];
    int bytes_read, bytes_written, bytes_total = 0;
    struct client *c;
    struct timespec sleep_time;

    sleep_time.tv_nsec = (BUF_SIZE / SAMPLE_SIZE) * 1000;

    if ((file_fd = open(filename, O_RDONLY)) == -1) {
        perror("Unable to open file");
        return NULL;
    }

    while (1) {
        if (c_head) {

            /* read chunk from file */

            while ((bytes_read = read(file_fd, buffer, BUF_SIZE)) == -1
                    && errno == EINTR);
            if (bytes_read == -1) { /* real error */
                perror("Error on reading from file");
                return NULL;
            }
            if (bytes_read == 0)    /* EOF */
                break;

            /* write chunk to each client */

            pthread_mutex_lock(&client_list_lock);
            for (c = c_head; c != NULL; c = c->next) {
                p_buffer = buffer;

                while (bytes_read > 0) {
                    while ((bytes_written = write(c->fd, p_buffer, bytes_read)) == -1
                            && errno == EINTR);
                    if (bytes_written == -1) {
                        fprintf(stderr, "Error on writing to client\n");
                        c = remove_client(c);
                        break;
                    }
                    if (bytes_written == 0)
                        break;

                    p_buffer += bytes_written;
                    bytes_read -= bytes_written;
                }
            }
            pthread_mutex_unlock(&client_list_lock);
        }

        nanosleep(&sleep_time, NULL);
    }

    done = 1;
    return NULL;
}

struct client *remove_client(struct client *c) {
    struct client *x;

    if (c == c_head)
        c_head = c->next;
    else {
        for (x = c_head; x->next != c; x = x->next);  /* find previous node */
        x->next = c->next;
    }

    close(c->fd);
    free(c);
    return x;
}

struct client *add_client(int fd) {
    struct client *c, *d;
    c = malloc(sizeof(struct client));
    c->fd = fd;
    c->next = NULL;

    if (c_head == NULL)
        c_head = c;
    else {
        d = c_head;
        for (; d->next != NULL; d = d->next);
        d->next = c;
    }

    return c;
}

void destroy_clients(void) {
    struct client *c = c_head, *d;

    while (c != NULL) {
        d = c->next;
        close(c->fd);
        free(c);
        c = d;
    }
}
