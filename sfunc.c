/*
 *
 *		sfunc.c - standard functions for iddb
 *
 */

#include "iddb.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

// Return 0 if string starts with key
int strst(const char *str, const char *key) {

	size_t klen = strlen(key);
	unsigned int a = 0;

	for(a = 0; a < klen; a++) { if(toupper(str[a]) != key[a]) return 1; }

	return 0;
}

// Creates a random string of length len
int randstr(char *str, const int len) {

	char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	int cslen = strlen(charset);
	unsigned int a = 0;

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

// Overflow protected atoi returning -1 or -2 on error
int matoi(const char *str) {

	size_t len = strlen(str);
	unsigned int a = 0;
	char **dump = calloc(SBCH, sizeof(char));

	for(a = 0; a < len; a++) { if(!isdigit(str[a])) return -1; }
	long lret = strtol(str, dump, SBCH);
	free(dump);

	if(lret <= INT_MAX && lret >= INT_MIN) return (int)lret;
	else return -2;
}

// Randomize timer
void setsrand() {

	time_t t;
	srand((unsigned) time(&t));
}

// Array to string (TODO: It's hacky. Review)
char *atostr(char *str, char **arr, const int num) {

	unsigned int a = 0, lsz = BBCH;

	for(a = 0; a < num; a++) {
		strncat(str, arr[a], lsz);
		str[strlen(str)] = ' ';
		lsz -= (strlen(arr[a]) + 1);
	}
	str[(strlen(str) - 1)] = '\0';

	return str;
}

// Escape str by prefixing char esc
char *esccpy(char *dest, char *src, const char esc,
	const char pref, size_t mxl) {

	size_t len = strlen(src);
	unsigned int a = 0, b = 0;

	if(!strchr(src, esc)) return src;

	for(a = 0; a < len; a++) {
		if(a + b >= mxl) return NULL;
		if(src[a] == esc) dest[(a + b++)] = pref;
		dest[(a + b)] = src[a];
	}

	return dest;
}
