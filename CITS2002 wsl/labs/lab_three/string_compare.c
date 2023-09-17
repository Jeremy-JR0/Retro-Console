#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

int my_strcmp(char string1[], char string2[]) {
    int length1 = strlen(string1);
    int length2 = strlen(string2);
    
    int i = 0;
    int j = 0;

    while (string1[i] != '\0' && string2[j] != '\0') {
        if (((int)string1[i]) == ((int)string2[j])) {
            i++;
            j++;
            continue;
        }
        else if (((int)string1[i]) < ((int)string2[j])) {
            return -1;
        }
        else {
            return 1;
        }
    }

    if (length1 == length2) {
        return 0;
    }

    else if (length1 < length2) {
        return -1;
    }
    else {
        return 1;
    }
}

int main(int argc, char *argv[]) {
    int compare = my_strcmp(argv[1], argv[2]);
    if (compare == -1) {
        printf("First string was lexigraphically smaller.\n");
    }
    else if (compare == 1) {
        printf("Second string was lexigraphically smaller.\n");
    }
    else {
        printf("Strings are equivalent.\n");
        }
}