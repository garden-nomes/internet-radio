#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    int fd, port, bytes_read, error;
    struct hostent *server;
    struct sockaddr_in serv_addr;
    char buffer[BUF_SIZE];
    pa_simple *pa_stream;
    static const pa_sample_spec spec = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };

    /* usage */

    if (argc < 3 || (port = atoi(argv[2])) < 0) {
        fprintf(stderr, "Usage: %s server port\n", argv[0]);
        return 1;
    }

    /* initialize networking */

    signal(SIGPIPE, SIG_IGN);

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Creating socket");
        return 1;
    }

    if ((server = gethostbyname(argv[1])) == NULL) {
        fprintf(stderr, "Error: host not found (%s)\n", argv[1]);
        return 1;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);

    /* connect to server */

    if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Error on connect");
        return 1;
    }

    /* initialize PulseAudio */

    pa_stream = pa_simple_new(NULL, "Internet Radio Client", PA_STREAM_PLAYBACK, NULL, "playback",
        &spec, NULL, NULL, &error);

    if (!pa_stream) {
        fprintf(stderr, "Error on pa_simple_new(): %s\n", pa_strerror(error));
        close(fd);
        return 1;
    }

    /* read from server */

    while ((bytes_read = read(fd, buffer, BUF_SIZE)) > 0) {
        if (pa_simple_write(pa_stream, buffer, bytes_read, &error) < 0) {
            fprintf(stderr, "Error on pa_simple_write(): %s\n", pa_strerror(error));
            close(fd);
            return 1;
        }
    }
    if (bytes_read == -1)
        perror("Error on read from socket");

    /* make sure the stream is done playing */

    if (pa_simple_drain(pa_stream, &error) < 0) {
        fprintf(stderr, "Error on pa_simple_drain(): %s\n", pa_strerror(error));
        close(fd);
        return 1;
    }

    /* clean up */

    pa_simple_free(pa_stream);
    close(fd);
    return 0;
}
