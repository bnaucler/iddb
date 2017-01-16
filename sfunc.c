#include "iddb.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// Return 0 if string starts with key
int strst(const char *str, const char *key) {

	int klen = strlen(key);
	unsigned int a = 0;

	// Not sure if tolower is needed
	for(a = 0; a < klen; a++) {
		if(tolower(str[a]) != tolower(key[a])) return 1;
	}

	return 0;
}

// Creates a random string of length len
int randstr(char *str, const int len) {

	char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	time_t t;
	unsigned int a = 0;

	srand((unsigned) time(&t));

	for(a = 0; a < len; a++) str[a] = charset[(rand() % strlen(charset))];
	str[len] = '\0';

	return a;
}
