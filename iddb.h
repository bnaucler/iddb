/*
 *
 *        iddb - Identity database / VCF parser
 *        https://github.com/bnaucler/iddb
 *
 *        Written by: Björn W Nauclér 2017
 *        License: MIT
 *
 *        See README.md for more information
 *
 */

#ifndef IDDB_HEAD
#define IDDB_HEAD

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <sqlite3.h>

#define VER "0.4A"
#define DBNAME ".iddb.sl3"

#define CMDLEN 30

#define NALEN 128
#define ORLEN 128
#define ULEN 64
#define UCLEN 20

#define EMLEN 128
#define PHLEN 20

#define EMNUM 10
#define PHNUM 10

#define FBCH 4096
#define DBCH 2048
#define BBCH 512
#define MBCH 128
#define SBCH 32

#define EXPLEN 30
#define EXPSUF ".vcf"

#define STARTKEY "BEGIN:VCARD"
#define STOPKEY "END:VCARD"
#define UIDKEY "UID"
#define ORGKEY "ORG"
#define EMKEY "EMAIL"
#define PHKEY "TEL"
#define FNKEY "FN"
#define ADDRKEY "ADR"
#define FRKEY "FROM:"

#define CITEMS 3
#define NUMCARD 1000

#define FCHAR '~'
#define ESCCHAR '\''
#define DDIV '/'

// Type definitions and enumerations
typedef enum op {create, delete, import, join, export, help, phone, raw, mail, new, all} op;
typedef enum svar {lid, uid, fn, org, em, ph} svar;

typedef struct card {
    int lid;
    char uid[ULEN];
    char fn[NALEN];
    char org[ORLEN];
    char ph[PHNUM][PHLEN];
    char em[EMNUM][EMLEN];
    unsigned int phnum;
    unsigned int emnum;
    struct card *next;
} card;

typedef struct flag {
    int op;
    int mxnum;
    int vfl;
    int sfl;
    char cmd[CMDLEN];
    char dfl[MBCH];
    char nfl[NALEN];
    char pfl[PHLEN];
    char efl[EMLEN];
} flag;

// Forward declarations - marshal.c
extern char *marshal(char *mstr, int rows, int cols, char arr[][cols]);
extern char **unmarshal(const char *str, char **arr);

// Forward declarations - sfunc.c
extern int strst(const char *str, const char *key);
extern int randstr(char *str, const size_t len);
extern int readline(char *prompt, char *buf, const char *def, const size_t mxl);
extern char *robj(char *buf, const size_t mxl);
extern int matoi(const char *str);
extern void setsrand();
extern char *atostr(char *str, char **arr, const size_t num);
extern char *esccpy(char *dest, const char *src, const char esc,
    const char pref, size_t mxl);
extern int mkpath(char *out, char *dirname, const char *fname,
    const char div, size_t mxl);
extern int isemail(const char *str);
extern int isphone(const char *str, const int mxl);
extern char *formphone(char *output, const char *input);
extern char *remchar(char *str, const char ch);
extern char *remtchar(char *str, const char ch);
extern int inarr(const char *str, const size_t mxl,
    char arr[][mxl], const size_t asz);
extern int selstr(const char *s1, const char *s2);
extern void selwr(char *s1, const char *s2, size_t mxl);

// Forward declarations - cfunc.c
extern int valcard(card *c);
extern int mvcard(sqlite3 *db, int plid, int nlid);
extern int cmpcard(const card *c1, const card *c2);
extern int printcard(card *c, const flag *f);
extern int wrcard(sqlite3 *db, card *c, const int op, const int verb);
extern int getindex(sqlite3 *db, const int verb);
extern int joincard(card *c1, const card *c2);

#endif
