/*
 *
 *        sfunc.c - standard functions for iddb
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

    do { if(*str != *key) return 1; 
    } while(*++str && *++key);

    return 0;
}

// Creates a random string of length len
int randstr(char *str, const size_t len) {

    char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int cslen = strlen(charset);
    unsigned int a = 0;

    for(a = 0; a < len; a++) str[a] = charset[(random() % cslen)];
    str[len] = 0;

    return a;
}

// Read line from tty
int readline(char *prompt, char *buf, const char *def, const size_t mxl) {

    char pstr[MBCH];

    FILE *tty = fopen("/dev/tty", "r");
    if(buf[0]) memset(buf, 0, mxl);

    if(def[0]) snprintf(pstr, MBCH, "%s (Default: %s)", prompt, def);
    else strncpy(pstr, prompt, MBCH);

    if(pstr[0]) printf("%s: ", pstr);
    fgets(buf, mxl, tty);

    buf[(strlen(buf) - 1)] = 0;

    fclose(tty);

    if(!buf[0]) return 1;
    else return 0;
}

// Return string between : and \n
char *robj(char *buf, const size_t mxl) {

    char *tbuf;

    tbuf = strtok(buf, ":");
    tbuf = strtok(NULL, "\n");

    if(!tbuf) {
        strncpy(buf, "UNKNOWN", mxl);

    } else {
        int p = strlen(tbuf);
        if(isspace(tbuf[0])) memmove(tbuf, tbuf + 1, p--);
        while(!isalnum(tbuf[p])) tbuf[p--] = '\0';
        strncpy(buf, tbuf, mxl);
    }

    return buf;
}

// Overflow protected atoi returning -1 or -2 on error
int matoi(const char *str) {

    size_t len = strlen(str);
    unsigned int a = 0;

    if(len > 7) return -1;
    for(a = 0; a < len; a++) { if(!isdigit(str[a])) return -2; }

    long lret = strtol(str, NULL, 10);

    if(lret <= INT_MAX && lret >= INT_MIN) return (int)lret;
    else return -2;
}

// Array to string (TODO: It's hacky. Review)
char *atostr(char *str, char **arr, const size_t num) {

    unsigned int a = 0, lsz = BBCH;

    if(num == 1) {
        strncpy(str, arr[0], BBCH);
        return str;
    }

    for(a = 0; a < num; a++) {
        strncat(str, arr[a], lsz);
        str[strlen(str)] = ' ';
        lsz -= (strlen(arr[a]) + 1);
    }
    str[(strlen(str) - 1)] = 0;

    return str;
}

// Escape str by prefixing char esc
char *esccpy(char *dest, const char *src, const char esc,
    const char pref, size_t mxl) {

    if(!strchr(src, esc)) {
        strcpy(dest, src);

    } else {
        size_t len = strlen(src);
        unsigned int a = 0, b = 0;

        for(a = 0; a < len; a++) {
            if(a + b >= mxl) return NULL;
            if(src[a] == esc) dest[(a + b++)] = pref;

            if(src[a] == '/' || !isalnum(src[a])) dest[(a + b)] = '_';
            else dest[(a + b)] = src[a];
        }
    }

    return dest;
}

// Create full path from dir- and file names
int mkpath(char *out, char *dirname, const char *fname,
    const char div, size_t mxl) {

    int arlen = strlen(dirname);

    if(arlen + strlen(fname) + 2 > mxl) return 1;

    if(dirname[(arlen - 1)] != div) {
        dirname[(arlen)] = div;
        dirname[(arlen + 1)] = '\0';
    }

    snprintf(out, MBCH, "%s%s", dirname, fname);

    return 0;
}

// Return 1 if string looks like an email address
int isemail(const char *str) {

    unsigned int a = 0, chk = 0, lpos = 0;
    int slen = strlen(str);

    for(a = 0; a < slen; a++) {
        if(chk > 1) {
            return 1;

        } else if(a > 0 && !lpos && str[a] == '@')  {
            lpos = a;
            chk++;

        } else if(str[a] == '@') {
            return 0;

        } else if(chk && a > lpos + 1 && str[a] == '.') {
            chk++;
        }
    }

    return 0;
}

// Return 1 if string looks like a phone number
int isphone(const char *str, const int mxl) {

    unsigned int a = 0, b = 0, inchr = 0;

    char chr[] = "+-()0123456789";

    int slen = strlen(str);
    int clen = strlen(chr);

    if(slen > mxl) return 0;

    for(a = 0; a < slen; a++) {
        if(isspace(str[a])) continue;
        else if(a > 0 && str[a] == chr[0]) return 0;

        for(b = 0; b < clen; b++) {
            if(str[a] == chr[b]) {
                inchr++;
                break;
            }
        }

        if(!inchr) return 0;
        inchr = 0;
    }

    return 1;
}

// Format phone number to +XXXXXXXX format
char *formphone(char *dest, const char *src) {

    int a = 0, b = 0;
    int slen = strlen(src);

    for(a = 0; a < slen; a++) {
        if(isspace(src[a]) || src[a] == '-' ||
                src[a] == '(' || src[a] == ')') b++;
        else dest[(a - b)] = src[a];
    }

    dest[(a - b)] = 0;

    if(dest[0] == '0' && dest[1] == '0') {
        dest++;
        dest[0] = '+';
    }

    return dest;
}

// Remove all occurances of ch from str
char *remchar(char *str, const char ch) {

    char *src, *dest;

    for (src = dest = str; *src; src++) {
        if (*src != ch) *dest++ = *src;
    }

    *dest = 0;

    return str;
}

// Remove all trailing ch from str
char *remtchar(char *str, const char ch) {

    int slen = strlen(str);

    unsigned int a = 0;

    for(a = slen - 1; a > 0; a--) {
        if(str[a] == ch) str[a] = 0;
        else break;
    }

    return str;
}

// Check if string exists in array
int inarr(const char *str, const size_t mxl,
    char arr[][mxl], const size_t asz) {

    unsigned int a = 0;

    for(a = 0; a < asz; a++) {
        if(!strncmp(str, arr[a], mxl)) return 0;
    }

    return 1;
}

// Interactively select string
int selstr(const char *s1, const char *s2) {

    char ans[3];

    printf("1: %s\n", s1);
    printf("2: %s\n", s2);

    if(readline("Keep (1/2)", ans, "1", 3)) return 1;
    else return matoi(ans);
}

// Loop wrapper for selstr
void selwr(char *s1, const char *s2, size_t mxl) {

    int rc = 0;

    if(!strncmp(s1, s2, mxl)) return;

    do rc = selstr(s1, s2);
    while (rc != 1 && rc != 2);

    if(rc == 2) strncpy(s1, s2, mxl);
}
