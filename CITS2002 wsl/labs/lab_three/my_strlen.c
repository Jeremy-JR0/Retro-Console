//my method

#include <stdio.h>
#include <stdlib.h>

int my_strlen(char b[]) {
    int length = 0;

    while ((b[length] != '\0')) {
        length++;
    }

    return length;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        exit(EXIT_FAILURE);
    }

    printf("array is %i elements long \n", my_strlen(argv[1]));
    return 0;
}

//chatGPT method using string library
/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        exit(EXIT_FAILURE);
    }

    printf("array is %zu long \n", strlen(argv[1]));

    return 0;
}
*/

// TRIAL #1 - does not work as sizeof within a function returns size of the pointer not the array
/*
#include <stdio.h>
#include <stdlib.h>

int my_strlen(char b[]) {
    char str[] = &b;
    return (sizeof(b)/sizeof(b[0]));
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        exit(EXIT_FAILURE);
    }

    printf("array is %i elements long \n", my_strlen(argv[1]));
    return 0;
}
*/