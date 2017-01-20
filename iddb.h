/*
 *
 *		iddb - Identity database / VCF parser
 *
 *		Written by: Björn W Nauclér 2017
 *		License: MIT
 *
 *		See README.md for more information
 *
 */

#ifndef IDDB_HEAD
#define IDDB_HEAD

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <sqlite3.h>

#define VER "0.1A"
#define DBNAME "iddb.sl3"

#define NALEN 128
#define ORLEN 128
#define ULEN 64
#define UCLEN 20

#define EMLEN 128
#define PHLEN 20

#define EMNUM 10
#define PHNUM 10

#define DBCH 2048
#define BBCH 512
#define MBCH 128
#define SBCH 32

#define EXPLEN 20
#define EXPSUF ".vcf"
#define EXPDIR "exp/"

#define STARTKEY "BEGIN:VCARD"
#define STOPKEY "END:VCARD"
#define UIDKEY "UID"
#define ORGKEY "ORG"
#define EMKEY "EMAIL"
#define PHKEY "TEL"
#define FNKEY "FN"
#define ADDRKEY "ADR"

#define CITEMS 3
#define NUMCARD 10

#define FCHAR '~'

// Global variables
typedef enum op {create, delete, import, export, help, phone, mail, new, all} op;
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
} card;

// Forward declarations - marshal.c
char *marshal(char *mstr, int rows, int cols, char arr[][cols]);
char **unmarshal(char *str, char **arr);

// Forward declarations - sfunc.c
int strst(const char *str, const char *key);
int randstr(char *str, const int len);
int readline(char *prompt, char *buf, const int mxlen);
char *robj(char *buf, const char *key);
int matoi(char *str);

#endif
