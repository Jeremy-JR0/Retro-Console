#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

int comparison(int one, int two) {
    if (one > two) {
        return one;
    }
    else {
        return two;
    }
}

/*
int maximum(int one, int two, int three) {
    int max_one;
    int max_two;
    max_one = comparison(one, two);
    max_two = comparison(one, three);
    if (max_one == max_two) {
        return one;
    }
    else {
        return comparison(two, three);
    }
}
*/

int main(int argc, char *argv[]) {
    if (argc < 3) {
        exit(EXIT_FAILURE);
    }

    int max = atoi(argv[1]);

    for (int i = 2; i < argc; i++) {
        max = comparison(max, atoi(argv[i]));
    }

    printf("%i\n\n", max);
}
