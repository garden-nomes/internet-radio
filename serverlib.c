#include <unistd.h>
#include <errno.h>

#define BUF_SIZE 1024

int write_file(int fromfd, int tofd) {
    /* adapted from USP book's copyfile */

    char *p_buffer;
    char buffer[BUF_SIZE];
    int bytes_read, bytes_written, bytes_total = 0;

    while (1) {
        /* read while handling interrupts */
        while ((bytes_read = read(fromfd, buffer, BUF_SIZE)) == -1
                && errno == EINTR);
        if (bytes_read == -1) { /* real error */
            perror("Error on reading from file");
            return -1;
        }
        if (bytes_read == 0)    /* EOF */
            break;

        p_buffer = buffer;
        while (bytes_read > 0) {
            while ((bytes_written = write(tofd, p_buffer, bytes_read)) == -1
                    && errno == EINTR);
            if (bytes_written == -1) {
                perror("Error on writing to socket");
                return -1;
            }
            if (bytes_written == 0)
                break;

            p_buffer += bytes_written;
            bytes_read -= bytes_written;
            bytes_total += bytes_written;
        }

        if (bytes_written == -1)
            break;
    }

    return bytes_total;
}
