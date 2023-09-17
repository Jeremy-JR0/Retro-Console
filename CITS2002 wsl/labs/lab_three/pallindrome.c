#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>


bool isPalindrome(char word[]) {
    int length = strlen(word);
    
    for (int i = 0; i < length/2; i++) {
        if (word[i] != word[length - 1 - i]) {
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    if (isPalindrome(argv[1])) {
        printf("is a palindrome\n");
    }
    else {
        printf("is not palindrome\n");
    }
}