#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void replace(char oldword[], char newword[], char whole_sentence[]) {
    int i = 0;
    int j, k;

    int oldword_length = strlen(oldword);
    int newword_length = strlen(newword);
    int sentence_length = strlen(whole_sentence);
    
    char newSentence[sentence_length + 1];
    int newSentenceIndex = 0;

    while (i < sentence_length) {
        j = 0;
        k = i;

        while (oldword[j] != '\0' && oldword[j] == whole_sentence[k]) {
            j++;
            k++;
        }

        if (j == oldword_length) {
            for (j = 0; j < newword_length; j++) {
                newSentence[newSentenceIndex++] = newword[j];
            }
            i += oldword_length;
        } else {
            newSentence[newSentenceIndex++] = whole_sentence[i++];
        }
    }

    newSentence[newSentenceIndex] = '\0';
    printf("%s\n", newSentence);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s oldword newword sentence\n", argv[0]);
        return EXIT_FAILURE;
    }

    replace(argv[1], argv[2], argv[3]);
    return EXIT_SUCCESS;
}
