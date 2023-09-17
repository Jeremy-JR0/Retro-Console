#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

void leftAlignment(int max) {
    for(int i = 1; i <= max; i++) {
        for(int j = 1; j <= i; j++) {
            printf("*");
        }
        printf("\n");
    }
}

void rightAlignment(int max) {
    for(int i = 1; i <= max; i++) {
        for(int j = 1; j <= (max - i); j++) {
            printf(" ");
        }
        for(int k = 1; k <= i; k++) {
            printf("*");
        }
        printf("\n");
    }
}

void upsideDown(int max) {
    for(int i = max; i >= 0; i--) {
        for(int j = i; j >= 0; j--) {
            printf("*");
        }
        printf("\n");
    }
}

void centre(int max) {
    for(int i = 1; i <= max; i++) {
        for(int j = 1; j <= (max - i); j++) {
            printf(" ");
        }
        for(int k = 1; k <= (i*2) - 1; k++) {
            printf("*");
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        exit(EXIT_FAILURE);
    }

    int max = atoi(argv[1]);
    leftAlignment(max);

    printf("\n\n\n");

    rightAlignment(max);
    
    printf("\n\n\n");

    upsideDown(max);

    printf("\n\n\n");

    centre(max);






    
}
