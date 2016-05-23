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

char *filename;

int write_file(int, int);   /* from serverlib.c */
int handle_client(int);

int main(int argc, char *argv[]) {
    int fd, client_fd, port, len_cli_addr;
    struct sockaddr_in serv_addr, cli_addr;
    const char *msg = "Hello from across teh interwebz!";
    pid_t pid;

    /* usage */

    if (argc < 3) {
        fprintf(stderr, "Usage: %s port filename\n", argv[0]);
        return 1;
    }
    port = atoi(argv[1]);
    filename = argv[2];

    /* initialize networking */

    signal(SIGPIPE, SIG_IGN);

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        return 1;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Error on binding");
        return 1;
    }

    if (listen(fd, 8) == -1) {
        perror("Error on listen");
        return 1;
    }

    len_cli_addr = sizeof(cli_addr);

    while (1) {
        /* accept client connection */
        client_fd = accept(fd, (struct sockaddr *)&cli_addr,
            (socklen_t *)&len_cli_addr);
        if (client_fd == -1) {
            perror("Error on accept");
            return 1;
        }

        /* start client handler */

        if ((pid = fork()) == -1)
            perror("Error on fork");
        else if (pid > 0) { /* parent */
            printf("[%ld] client process started\n", (long)pid);
            close(client_fd);
        } else { /* child */
            close(fd);
            handle_client(client_fd);

            printf("[%ld] client process ended\n", (long)getpid());
            close(client_fd);
            return 0;
        }
    }

    /* clean up */

    close(fd);
    return 0;
}

int handle_client(int fd) {
    int file_fd, n;

    if ((file_fd = open(filename, O_RDONLY)) == -1) {
        perror("Unable to open file");
        return -1;
    }

    if ((n = write_file(file_fd, fd)) == -1)
        return -1;

    return 0;
}
