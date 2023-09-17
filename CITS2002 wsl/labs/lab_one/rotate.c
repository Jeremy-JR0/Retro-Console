#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Compile this program with:
//    cc -std=c11 -Wall -Werror -o rotate rotate.c

// #define ROT 3

//  The rotate function returns the character ROT positions further along the
//  alphabetic character sequence from c, or c if c is not lower-case

char rotate(char c, int rot)
{
    // Check if c is lower-case or not
    if(islower(c)) {
        // The ciphered character is ROT positions beyond c,
        // allowing for wrap-around
    return ('a' + (c - 'a' + rot) % 26);
    }
   
    else {
        return ('A' + (c - 'A' + rot) % 26);
    }
    
}

int characterType(char c) {
    return isdigit(c);
}

//  Execution of the whole program begins at the main function

int main(int argcount, char *argvalue[])
{
    int rot = 13;
    // Exit with an error if the number of arguments (including
    // the name of the executable) is not precisely 2
    if(argcount < 3) {
        fprintf(stderr, "%s: program expected 1 argument, but instead received %d\n",
                    argvalue[0], argcount-1);
        exit(EXIT_FAILURE);
    }
    else {
        for (int j = 2; j <= (argcount-1); j++) {
            
        // Calculate the length of the first argument
        int length = strlen(argvalue[j]);

        for(int i = 0; i < length; i++) {
            int num = characterType(argvalue[j][i]);
            if(num > 0) {
                rot = num;
                break;
            }
        }

        // Loop for every character in the text
        for(int i = 0; i < length; i++) {
            // Determine and print the ciphered character
            printf("%c", rotate(argvalue[j][i],  rot));
            printf("|");
            printf("%c", argvalue[j][i]);
            printf("|");
            printf("%i", i);
            printf("\n");
        }
        printf("\n");
    }

        // Print one final new-line character
        printf("\n");

        // Exit indicating success
        exit(EXIT_SUCCESS);
    }
    return 0;
}