/*
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>


int odd(int cardNumber, int length) {
    int number = cardNumber;
    int oddNumber = 0;

    while (number != 0) {
        oddNumber += (number % 10);
        number /= 100;
    }
    return oddNumber;
}

int even(int cardNumber, int length) {
    int number = cardNumber;
    number /= 10;

    int evenNumber = 0;
    int tempNum = 0;

    while (number != 0) {
        tempNum = 2 * (number % 10);

        // Split the product's digits and add them to evenNumber
        if (tempNum >= 10) {
            evenNumber += (tempNum % 10);  // Add the last digit
            tempNum /= 10;
        }
        evenNumber += tempNum;  // Add the first (or the only) digit

        number /= 100;
    }
    return evenNumber;
}

bool numCheck(int s1, int s2) {

    return (((s1 + s2) % 10) == 0);
}

int main(int argc, char *argv[]) {

    int length = strlen(argv[1]);

    int cardNumber = atoi(argv[1]);

    int s1 = odd(cardNumber, length);
    int s2 = even(cardNumber, length);

    if (numCheck(s1, s2)) {
        printf("Valid credit card\n");
    }
    else {
        printf("Not valid credit card\n");
    }
}
*/

/*
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

int calculateOddSum(int cardNumber) {
    int oddSum = 0;

    while (cardNumber > 0) {
        oddSum += cardNumber % 10;
        cardNumber /= 100;
    }

    return oddSum;
}

int calculateEvenSum(int cardNumber) {
    int evenSum = 0;

    while (cardNumber > 0) {
        int digit = (cardNumber % 10) * 2;
        if (digit >= 10) {
            digit = digit % 10 + digit / 10;
        }
        evenSum += digit;
        cardNumber /= 100;
    }

    return evenSum;
}

bool luhnCheck(int cardNumber) {
    int oddSum = calculateOddSum(cardNumber / 10);
    int evenSum = calculateEvenSum(cardNumber / 10);
    return (oddSum + evenSum) % 10 == 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <credit_card_number>\n", argv[0]);
        return 1;
    }

    int cardNumber = atoi(argv[1]);
    if (cardNumber <= 0) {
        printf("Invalid credit card number\n");
        return 1;
    }

    if (luhnCheck(cardNumber)) {
        printf("Valid credit card\n");
    } else {
        printf("Not valid credit card\n");
    }

    return 0;
}
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

//  THE FUNCTION RECEIVES AN ARRAY OF CHARACTERS, WITHOUT SPECIFYING ITS LENGTH
bool Luhn(char creditcard[])
{
    int  s1  = 0;
    int  s2  = 0;
    bool odd = true;

//  ITERATE OVER THE STRING BACKWARDS
    for(int i = strlen(creditcard)-1 ; i >= 0  ; i = i-1) {
        char digit = creditcard[i] - '0';

        if(odd) {
            s1 += digit;
        }
        else {
            int mult = digit*2;
            s2 += (mult/10) + (mult%10);
        }
        odd     = !odd;
    }
    return ((s1+s2) % 10) == 0;
}

int main(int argcount, char *argv[])
{
//  ITERATE OVER EACH COMMAND-LINE ARGUMENT
    for(int a=1 ; a<argcount ; ++a) {
        if(Luhn(argv[a])) {
            printf("%16s\t%s\n", argv[a], "OK");
        }
        else {
            printf("%16s\t%s\n", argv[a], "not OK");
        }
    }
    return 0;
}