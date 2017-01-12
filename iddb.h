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
#include <string.h>
#include <sqlite3.h>

#define VER "0.1A"
#define DBNAME "iddb.sl3"

#define NALEN 128
#define ORLEN 128
#define ULEN 64

#define EMLEN 128
#define PHLEN 20

#define EMNUM 10
#define PHNUM 10

#define DBCH 2048
#define BBCH 512
#define MBCH 128
#define SBCH 32

#define UIDKEY "UID"
#define ORGKEY "ORG"
#define EMKEY "EMAIL"
#define PHKEY "TEL"
#define FNKEY "FN"
#define ADDRKEY "ADR"

#define CITEMS 3
#define NUMCARD 10

#define FCHAR '~'

#endif
