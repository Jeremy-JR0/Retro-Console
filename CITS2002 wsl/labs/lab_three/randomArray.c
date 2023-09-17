#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

int fillRandom() {    
        return srand(time(NULL));
    }

int fillNotRandom(int array[]) {
    int i = 0;
    while (array[i] != '\0') {
        array[i] = srand();
        printf("element %i is %i \n", i, array[i]);
        i++;
    }
    return array;
}

int main(int argc, char *argv[]) {
    int arrayOneRand[10];
    int arrayTwo[10];

    while (array[i] != '\0') {
        arrayOneRandom[i] = fillRandom();
        arrayTwoRandom[i] = fillRandom();
        i++
    }
}