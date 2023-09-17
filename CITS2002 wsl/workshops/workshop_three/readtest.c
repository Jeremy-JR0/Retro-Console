#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <inttypes.h>
#include <sys/time.h>

//  RETURNS THE CURRENT TIME, AS A NUMBER OF MILLISECONDS, IN A 64-bit INTEGER
int64_t milliseconds(void)
{
    struct timeval  now;

    gettimeofday( &now, NULL );       // timezone not required, so we pass NULL
    return ((int64_t)now.tv_sec * 1000) + now.tv_usec/1000;
}

void readfile(char filename[], size_t bufferSize) {
    int n_syscalls = 0;
    int64_t start = milliseconds();

    int fd = open(filename, O_RDONLY);
    n_syscalls++;

    if (fd >= 0) {
        char buffer[bufferSize];
        size_t got;

        while((got = read(fd, buffer, sizeof(buffer))) > 0) {
            n_syscalls++;
        }
        close(fd);
        n_syscalls++;
    }

    if (n_syscalls > 0) {
        printf("%zu\t%i\n", bufferSize, (int)(milliseconds() - start) );
    }
}

void usage(char progname[]) {
    printf("Usage: %s filename buffersize\n", progname);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage(argv[0]);
    }

    size_t bufferSize = atoi(argv[2]);
    if (bufferSize <= 0) {
        usage(argv[0]);
    }

    readfile(argv[1], bufferSize);

    return EXIT_SUCCESS;
}