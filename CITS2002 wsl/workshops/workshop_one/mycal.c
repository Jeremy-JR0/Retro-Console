#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_MONTH 8
#define DEFAULT_YEAR 2023

#define N_WEEKS 6
#define N_DAYS_PER_WEEK 7

/*
     August 2023    
Su Mo Tu We Th Fr Sa
       1  2  3  4  5
 6  7  8  9 10 11 12
13 14 15 16 17 18 19
20 21 22 23 24 25 26
27 28 29 30 31

*/

void print_cal(int month, int year) {
	//printf("%i\n",  year);
	int days_in_month;
	int day1 = 2;
	switch(month) {
		case 1: printf("January %i\n", year);
			days_in_month = 31;
			break;
		case 2: printf("February %i\n", year);
			days_in_month = 28;
			break;
		case 3: printf("March %i\n", year);
			days_in_month = 31;
			break;
		case 4: printf("April %i\n", year);
			days_in_month = 30;
			break;
		case 5: printf("May %i\n", year);
			days_in_month = 31;
			break;
		case 6: printf("June %i\n", year);
			days_in_month = 30;
			break;	
		case 7: printf("July %i\n", year);
			days_in_month = 31;
			break;
		case 8: printf("August %i\n", year);
			days_in_month = 31;
			break;
		case 9: printf("September %i\n", year);
			days_in_month = 30;
			break;
		case 10: printf("October %i\n", year);
			days_in_month = 31;
			break;
		case 11: printf("November %i\n", year);
			days_in_month = 30;
			break;
		case 12: printf("December %i\n", year);
			days_in_month = 31;
			break;
	}

	printf("Su Mo Tu We Th Fr Sa\n");
	int day = 1 - day1;
	for(int w=0; w<N_WEEKS; ++w) {
		for(int d=0; d<N_DAYS_PER_WEEK; ++d) {
		if(day < 1|| day > days_in_month) {
			printf(".. ");
		}
		else {
			printf("%2i ", day);
		}
		day++;
		}
		printf("\n");
	}
}

int main(int argc, char *argv[]) {

	int month = DEFAULT_MONTH;
	int year = DEFAULT_YEAR;


	print_cal(month, year);
	exit(EXIT_SUCCESS);
	return EXIT_SUCCESS;

}

