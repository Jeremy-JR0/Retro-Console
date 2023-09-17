#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

bool leap_check(int year) {
    if((year%400 == 0)||
    ((year%4 == 0) && (year%100 != 0))) {
        return true;
    }
    else {
        return false;
    }
}

int main(int argc, char *argv[]) {
    
    int year = atoi(argv[1]);

    if (leap_check(year)) {
        printf("%i is a leap year\n", year);
    }
    else {
        printf("%i not leap year\n", year);
    }


}