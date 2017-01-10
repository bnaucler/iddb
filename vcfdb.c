/*
 *
 *		vcfdb.c - vCard database
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sqlite3.h>

#define VER "0.1A"
#define DBNAME "vcfdb.sl3"

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

typedef struct card {
	unsigned int lid;
	char uid[ULEN];
	char fn[NALEN];
	char ln[NALEN];
	char org[ORLEN];
	char ph[PHNUM][PHLEN];
	char em[EMNUM][EMLEN];
	unsigned int phnum;
	unsigned int emnum;
} card;

// Actual operations
typedef enum op {create, import, export, phone, email} op;

// Operations for string comparison
char vops[5][7] = {"create", "import", "export", "phone", "email"};

int usage(char *cmd) {

	if (errno) perror("Error");

	printf("%s %s - usage:\n", cmd, VER);
	printf("%s [args] command file/string\n", cmd);
	printf("Possible commands are:\n");
	printf("c[reate], i[mport], e[xport], p[hone], e[mail], a[ll]\n");

	exit(errno);
}

// Check for valid operation
static int chops(char *cop) {

	int opnum = sizeof(vops) / sizeof(vops[1]);
	int oplen = strlen(cop);

	unsigned int a;

	for(a = 0; a < opnum; a++) {
		if(oplen == 1) {
			if(cop[0] == vops[a][0]) return a;
		}
		else {
			if(strcmp(cop, vops[a]) == 0) return a;
		}
	}

	errno = EINVAL;
	return -1;
}

// Return 0 if string starts with key
int strst(char *str, char *key) {

	int klen = strlen(key);
	unsigned int a = 0;

	// Not sure if tolower / ctype is needed
	for(a = 0; a < klen; a++) {
		if(tolower(str[a]) != tolower(key[a])) return 1;
	}

	return 0;
}

// Format string based on object key
char *robj(char *buf, char *key) {

	buf = strtok(buf, ":");
	buf = strtok(NULL, "\n");

	return buf;
}

// Import card to struct
card *icard(card *ccard, FILE *f) {

	char *buf = malloc(MBCH);
	ccard->phnum = 0;
	ccard->emnum = 0;

	while(fgets(buf, MBCH, f)){
		if(!strst(buf, UIDKEY))
			strncpy(ccard->uid, robj(buf, UIDKEY), ULEN);
		if(!strst(buf, FNKEY))
			strncpy(ccard->fn, robj(buf, FNKEY), NALEN);
		if(!strst(buf, EMKEY)) 
			strncpy(ccard->em[ccard->emnum++], robj(buf, EMKEY), EMLEN);
		if(!strst(buf, PHKEY)) 
			strncpy(ccard->ph[ccard->phnum++], robj(buf, PHKEY), PHLEN);
	}

	free(buf);
	return ccard;
}

// Create (or reset) database table
int ctable(sqlite3 *db) {

	char *sql = calloc(DBCH, sizeof(char));
	char *err = 0;
	int dbok = 0;

	strncpy(sql, "DROP TABLE IF EXISTS id;"
			"CREATE TABLE id(lid INT, uid TEXT, fn TEXT);", DBCH);
	dbok = sqlite3_exec(db, sql, 0, 0, &err);

	if(!dbok) {
		strncpy(sql, "DROP TABLE IF EXISTS ctr;"
			"CREATE TABLE ctr(num, INT);", DBCH);
		dbok = sqlite3_exec(db, sql, 0, 0, &err);
	}

	return dbok;
}

// Write struct to DB
int wrdb(sqlite3 *db, card *ccard) {

	char *sql = calloc(BBCH, sizeof(char));
	char *err = 0;

	printf("uid: %s\n", ccard->uid);
	printf("fn: %s\n", ccard->fn);

	// Temporary fix
	ccard->fn[(strlen(ccard->fn) - 1)] = '\0';
	ccard->uid[(strlen(ccard->uid) - 1)] = '\0';

	snprintf(sql, BBCH, "INSERT INTO id VALUES(%d, \'%s\', \'%s\');",
			ccard->lid++, ccard->uid, ccard->fn);

	printf("query: %s\n", sql);

	return sqlite3_exec(db, sql, 0, 0, &err);
}

// Return entry from database
card *searchdb(sqlite3 *db, card *ccard, sqlite3_stmt *stmt, char *str) {

	char *sql = calloc(BBCH, sizeof(char));
		
	snprintf(sql, BBCH, "SELECT * FROM id");

	int dbok = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	unsigned int a = 0;

	if (dbok) printf("error# %d\n", dbok);

	while((dbok = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(dbok == SQLITE_ROW) {
			int ccnt = sqlite3_column_count(stmt);
			for(a = 0; a < ccnt; a++){
				if(strcmp("fn", sqlite3_column_name(stmt, a)) == 0)
					strcpy(ccard->fn, (char*)sqlite3_column_text(stmt, a));
			}
		}
	}

	printf("%s\n", ccard->fn);
	return ccard;
}

// Marshal array to string
// char *marsarr(char **arr, int offs) {

// 	int ni = sizeof(arr) / sizeof(arr[0]);
// 	int ti = 0;
// 	unsigned int a = 0;
// 	int pos = 0;

// 	for(a = 0; a < ni; a++) { 
// 		if(arr[a][0]) ti++;
// 		break;
// 	}

// 	// TODO: Fix it
// 	// char *mstr = calloc((ti * sizeof(arr[0]), sizeof(char));
// 	char *mstr;

// 	return mstr;
// }

int main(int argc, char *argv[]) {

	char *cmd = calloc(MBCH, sizeof(char));
	char *cop = calloc(MBCH, sizeof(char));

	sqlite3 *db;
	sqlite3_stmt *stmt;

	int op = 0;
	int dbok = 0;

	int optc;
	
	strncpy(cmd, basename(argv[0]), MBCH);

	while((optc = getopt(argc, argv, "a")) != -1) {
		switch (optc) {

			default:
				usage(cmd);
				break;
		}
	}

	// if(argc < optind + 2) errno = ENODATA;
	if(errno) usage(cmd);
	strncpy(cop, argv[optind], MBCH);
	op = chops(cop);

	dbok = sqlite3_open(DBNAME, &db);
	if(dbok) errno = ENOENT;
	// if(errno) usage(cmd);

	if(op == create) {
		if (ctable(db)) {
			errno = EEXIST;
			usage(cmd);
		} else {
			printf("ok\n");
				exit(0);
			}
	}

	card *ccard = calloc(1, sizeof(card));

	if(op == import) {
		FILE *f = fopen(argv[(optind + 1)], "r");
		if(f == NULL) {
			errno = ENOENT;
			usage(cmd);
		} else {
			icard(ccard, f);
			wrdb(db, ccard);
		}
	}

	// if(op == export) ecard();
	if(op == phone) searchdb(db, ccard, stmt, argv[(optind + 1)]);
	// if(op == email) searchdb();

	sqlite3_close(db);
	return 0;
}
