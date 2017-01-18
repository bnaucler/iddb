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
	int cslen = strlen(charset);
	time_t t;
	unsigned int a = 0;

	srand((unsigned) time(&t));

	for(a = 0; a < len; a++) str[a] = charset[(rand() % cslen)];
	str[len] = '\0';

	return a;
}

// Read line from stdin
int readline(char *prompt, char *buf, const int mxlen) {

	memset(buf, 0, mxlen);

	if(prompt) printf("%s: ", prompt);
	fgets(buf, mxlen, stdin);
	buf[(strlen(buf) - 1)] = '\0';

	if(!buf[0]) return 1;
	else return 0;
}

// Format string based on object key
char *robj(char *buf, const char *key) {

	buf = strtok(buf, ":");
	buf = strtok(NULL, "\n");

	int p = strlen(buf) - 1;
	while(!isalnum(buf[p])) buf[p--] = '\0';

	return buf;
}
