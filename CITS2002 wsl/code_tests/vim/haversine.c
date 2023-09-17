#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(__linux__)
#define __USE_XOPEN
#endif

#include <math.h>

#define MIN_LAT 90.0
#define MIN_LON 90.0
#define MAX_LON 180.0
#define MAX_LAT 180.0

#define EARTH_RADIUS_IN_METRES 6372797


/*
 Our goal is to develop 3 simple _functions_:

1. **double** degrees_to_radians(**double** degrees);
    
2. **bool** valid_location(**double** latitude, **double** longitude);
    
3. **double** haversine(**double** latitude1, **double** longitude1, **double** latitude2, **double** longitude2);
*/

double degrees_to_radians(double degrees) {

	return (degrees * M_PI / 180.0);
}

bool valid_location(double latitude, double longitude) {

	return ((latitude < MAX_LAT) | (latitude > MIN_LAT) | (longitude < MAX_LAT) | (longitude > MIN_LAT));
}

double haversine(double latitude1, double longitude1, double latitude2, double longitude2) {

	double deltalat = (latitude2 - latitude1) / 2.0;
	double deltalon = (longitude2 - longitude1) / 2.0;

	double sin1 = sin(degrees_to_radians(deltalat));
	double cos1 = cos(degrees_to_radians(latitude1));
	double cos2 = cos(degrees_to_radians(latitude2));
	double sin2 = sin(degrees_to_radians(deltalon));

	double x = sin1*sin1 + cos1*cos2 * sin2*sin2;
	return (2.0 * EARTH_RADIUS_IN_METRES * asin(sqrt(x)));
}




int main(int argc, char *argv[]) {

	if (argc != 5) {
		exit(EXIT_FAILURE);
	}

	double lat1 = atof(argv[1]);
	double lon1 = atof(argv[2]);
	double lat2 = atof(argv[3]);
	double lon2 = atof(argv[4]);

	if (! valid_location(lat1, lon1)) {
		exit(EXIT_FAILURE);
	}

	if (! valid_location(lat2, lon2)) {
		exit(EXIT_FAILURE);
	}

	printf("%im\n", (int)haversine(lat1, lon1, lat2, lon2));
	
	return 0;
}
