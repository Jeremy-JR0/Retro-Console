#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

bool isSafe(char password[]) {
    //safe[]: {Uppercase, Lowercase, Digits}
    int safe[3] = {0, 0, 0};
    int i = 0;

    while (password[i] != '\0') {
        if (isupper(password[i])) {
            safe[0]++;
        }
        else if (islower(password[i])) {
            safe[1]++;
        }
        else if (isdigit(password[i])) {
            safe[2]++;
        }

        i++;
    }

    return ((safe[0] >= 2) && (safe[1] >= 2) && (safe[2] >= 2));
}

int main(int argc, char *argv[]) {
    
    if (isSafe(argv[1])) {
        printf("Password is safe.\n");
    }
    else {
        printf("Password is not safe.\n");
    }

    return 0;
}