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
#define DBNAME "vcf.db"

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

typedef struct card {
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

	// char *sql = calloc(DBCH, sizeof(char));
	char *err = 0;

	char *sql = "CREATE TABLE id(uid, TEXT, fn TEXT);";

	return sqlite3_exec(db, sql, 0, 0, &err);
}

// Write struct to DB
int wrdb(sqlite3 *db, card *ccard) {

	char *sql = calloc(DBCH, sizeof(char));
	char *err = 0;

	snprintf(sql, DBCH, "INSERT INTO id VALUES(%s, %s)",
			ccard->uid, ccard->fn);

	return sqlite3_exec(db, sql, 0, 0, &err);
}

// Example callback
static int callback(void *data, int argc, char **argv, char **azColName){
   int i;

	printf("beep\n");

	fprintf(stderr, "%s: ", (const char*)data);
	for(i=0; i<argc; i++){
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	printf("\n");
	return 0;
}


// Return entry from database
card *searchdb(sqlite3 *db, card *ccard, char *str) {

	char *sql = "SELECT * FROM id";
	char *err = 0;
	const char* data = "Callback function called";

	int dbok = sqlite3_exec(db, sql, callback, (void*)data, &err);

	if (dbok) printf("error! %d\n", dbok);

	return ccard;
}

int main(int argc, char *argv[]) {

	char *cmd = calloc(MBCH, sizeof(char));
	char *cop = calloc(MBCH, sizeof(char));

	sqlite3 *db;

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
	// if(dbok) errno = ENOENT;
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
			printf("uid: %s\n", ccard->uid);
			printf("fn: %s\n", ccard->fn);
			printf("email 0: %s\n", ccard->em[0]);
			printf("email 1: %s\n", ccard->em[1]);
			wrdb(db, ccard);
		}
	}

	// if(op == export) ecard();
	if(op == phone) searchdb(db, ccard, argv[(optind + 1)]);
	// if(op == email) searchdb();

	sqlite3_close(db);
	return 0;
}
