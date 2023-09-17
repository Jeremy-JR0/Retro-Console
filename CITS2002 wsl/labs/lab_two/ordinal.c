#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[]) {
    int digitOne = 0;
    int temp = 0;
    for (int i = 1; i < argc; i++) {
        temp = atoi(argv[i]);
        digitOne = temp % 10;
        printf("%i", temp);
        if (digitOne == 1 && (((temp / 10) % 10) != 1)) {
            printf("st\n");
        }
        else if (digitOne == 2 && (((temp / 10) % 10) != 1)) {
            printf("nd\n");
        }
        else if (digitOne == 3 && (((temp / 10) % 10) != 1)) {
            printf("rd\n");
        }
        else {
            printf("th\n");
        }
    }
}