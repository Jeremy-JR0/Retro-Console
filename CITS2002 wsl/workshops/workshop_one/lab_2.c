#include <stdio.h>
#include <stdlib.h>
#include <time.h>

bool leap_check(int year) {
    if(year%400 == 0||(year%4 == 0 ^ year%100 != 0)) {
        return true;
    }
    else {
        return false;
    }
}

int main(int argc, char *argv) {
    int year = atoi(argv[1]);
    if (leap_check(year)) {
        printf("leap year");
    }
    else {
        printf("not leap year");
    }
}