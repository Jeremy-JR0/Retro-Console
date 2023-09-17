#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>


int depth = 0;

int ackermann(int a, int b, int *depth, int *calls) {
    (*calls)++;
    if (*depth < a) {
        *depth = a;
    }

    if (a == 0) {
        return (b+1);
    }

    else if (b == 0) {
        return ackermann(a - 1, 0, depth, calls);
    }
    
    else {
        return ackermann((a - 1), ackermann(a, b - 1, depth, calls), depth, calls);
    }
}

int main(int argc, char *argv[]) {
    if ((argc != 3)) {
        exit(EXIT_FAILURE);
    }

    int a = atoi(argv[1]);
    int b = atoi(argv[2]);
    int depth = 0;
    int calls = 0;
    int result = ackermann(a, b, &depth, &calls);

    printf("result is %i\n", result);
    printf("with %i calls and depth of %i\n", calls, depth);

    return 0;
}
