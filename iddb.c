/*
 *
 *		iddb.c - Identity database / VCF parser
 *
 */

#include "iddb.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sqlite3.h>

typedef struct card {
	unsigned int lid;
	char uid[ULEN];
	char fn[NALEN];
	char org[ORLEN];
	char ph[PHNUM][PHLEN];
	char em[EMNUM][EMLEN];
	unsigned int phnum;
	unsigned int emnum;
} card;

// Actual operations
typedef enum op {create, import, export, help, phone, email} op;

// Operations for string comparison
char vops[6][7] = {"create", "import", "export", "help", "phone", "email"};

int usage(char *cmd) {

	if (errno) perror("Error");

	printf("%s %s - usage:\n", cmd, VER);
	printf("%s [args] command file/string\n", cmd);
	printf("Possible commands are:\n");
	printf("c[reate], i[mport], e[xport], h[elp], p[hone], e[mail], a[ll]\n");

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

	// Not sure if tolower is needed
	for(a = 0; a < klen; a++) {
		if(tolower(str[a]) != tolower(key[a])) return 1;
	}

	return 0;
}

// Format string based on object key
char *robj(char *buf, char *key) {

	buf = strtok(buf, ":");
	buf = strtok(NULL, "\n");

	int p = strlen(buf) - 1;
	while(!isalnum(buf[p])) buf[p--] = '\0';

	return buf;
}

// Import card to struct
card *icard(card *cc, FILE *f) {

	char *buf = malloc(MBCH);
	cc->phnum = 0;
	cc->emnum = 0;

	while(fgets(buf, MBCH, f)){
		if(!strst(buf, UIDKEY))
			strncpy(cc->uid, robj(buf, UIDKEY), ULEN);
		if(!strst(buf, FNKEY))
			strncpy(cc->fn, robj(buf, FNKEY), NALEN);
		if(!strst(buf, EMKEY))
			strncpy(cc->em[cc->emnum++], robj(buf, EMKEY), EMLEN);
		if(!strst(buf, PHKEY))
			strncpy(cc->ph[cc->phnum++], robj(buf, PHKEY), PHLEN);
	}

	free(buf);
	return cc;
}

// Create (or reset) database table
int ctable(sqlite3 *db) {

	char *sql = calloc(DBCH, sizeof(char));
	char *err = 0;
	int dbok = 0;

	strncpy(sql, "DROP TABLE IF EXISTS id;"
		"CREATE TABLE id(lid INT, uid TEXT, fn TEXT,"
		" org TEXT, ph TEXT, em TEXT);", DBCH);
	dbok = sqlite3_exec(db, sql, 0, 0, &err);

	if(!dbok) {
		strncpy(sql, "DROP TABLE IF EXISTS ctr;"
			"CREATE TABLE ctr(num, INT);", DBCH);
		dbok = sqlite3_exec(db, sql, 0, 0, &err);
	}

	return dbok;
}

// Return marshalled string to array format
char **unmarshal(char *str, char **arr) {

	char *tmp = calloc(str[1] + 1, sizeof(char));
	unsigned int a = 0, b = 0, w = 2;

	for(a = 0; a < str[0]; a++) {
		for(b = 0; b < str[1]; b++) {
			if(str[w] != FCHAR) tmp[b] = str[w];
			w++;
		}
		if(tmp[0]) strncpy(arr[a], tmp, str[1]);
		memset(tmp, 0, str[1] + 1);
	}

	return arr;
}

// Convert array to string
char *marshal(char *mstr, int rows, int cols, char arr[][cols]) {

	unsigned int a = 0, b = 0, w = 2, ln = 0;
	mstr[0] = rows;

	for(a = 0; a < mstr[0]; a++) {
		ln = strlen(arr[a]);
		if(mstr[1] < ln) mstr[1] = ln;
	}

	for(a = 0; a < mstr[0]; a++) {
		for(b = 0; b < mstr[1]; b++) {
			if(arr[a][b]) mstr[w++] = arr[a][b];
			else mstr[w++] = FCHAR;
		}
	}

	mstr[w] = '\0';
	return mstr;
}

// Write struct to DB
int wrdb(sqlite3 *db, card *cc, int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	char *pbuf = calloc(PHNUM * PHLEN, sizeof(char));
	char *mbuf = calloc(EMNUM * EMLEN, sizeof(char));
	char *err = 0;

	printf("uid: %s\n", cc->uid);
	printf("fn: %s\n", cc->fn);
	printf("em 0: %s\n", cc->em[0]);
	printf("ph 0: %s\n", cc->ph[0]);

	snprintf(sql, BBCH, "INSERT INTO id VALUES"
			"(%d, '%s', '%s', '%s',  '%s', '%s');",
			cc->lid++, cc->uid, cc->fn, cc->org,
			marshal(pbuf, cc->phnum, PHLEN, cc->ph),
			marshal(mbuf, cc->emnum, EMLEN, cc->em));

	if (verb) printf("Query: %s\n", sql);

	return sqlite3_exec(db, sql, 0, 0, &err);
}

// Print card to stdout
void printcard(card *cc) {

	unsigned int a = 0;

	if(cc->uid[0]) printf("uid: %s\n", cc->uid);
	printf("fn: %s\n", cc->fn);
	if(cc->org[0]) printf("org: %s\n", cc->org);

	for(a = 0; a < cc->phnum; a++) printf("ph %d: %s\n", a, cc->ph[a]);
	for(a = 0; a < cc->emnum; a++) printf("em %d: %s\n", a, cc->em[a]);
}

// Return 0 if c1 and c2 are identical (TODO: implement)
int cmpcard(card *c1, card *c2) {

	unsigned int a = 0;

	if(c1->uid != c2->uid) return 1;
	if(c1->fn != c2->fn) return 2;
	if(c1->org != c2->org) return 3;
	if(c1->phnum != c2->phnum) return 4;
	if(c1->emnum != c2->emnum) return 5;

	for(a = 0; a < c1->phnum; a++) { if(c1->ph[a] == c2->ph[a]) return 6; }
	for(a = 0; a < c1->emnum; a++) { if(c1->em[a] == c2->em[a]) return 7; }

	return 0;
}

// Return entry from database
card *searchdb(sqlite3 *db, card *cc, char *str, int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	char *buf = calloc(BBCH, sizeof(char));
	char **tarr = calloc(EMNUM * EMLEN, sizeof(char));

	sqlite3_stmt *stmt;

	if(str == NULL) snprintf(sql, BBCH, "SELECT * FROM id;");
	else snprintf(sql, BBCH, "SELECT * FROM id WHERE fn LIKE '%%%s%%';", str);

	if (verb) printf("Query: %s\n", sql);

	int dbok = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	unsigned int a = 0, b = 0;

	if (dbok) printf("error# %d\n", dbok);

	while((dbok = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(dbok == SQLITE_ROW) {
			int ccnt = sqlite3_column_count(stmt);
			for(a = 0; a < ccnt; a++){
				if(strcmp("uid", sqlite3_column_name(stmt, a)) == 0)
					strcpy(cc->uid, (char*)sqlite3_column_text(stmt, a));

				if(strcmp("fn", sqlite3_column_name(stmt, a)) == 0)
					strcpy(cc->fn, (char*)sqlite3_column_text(stmt, a));

				if(strcmp("org", sqlite3_column_name(stmt, a)) == 0)
					strcpy(cc->org, (char*)sqlite3_column_text(stmt, a));

				if(strcmp("ph", sqlite3_column_name(stmt, a)) == 0) {
					strncpy(buf, (char*)sqlite3_column_text(stmt, a), BBCH);
					for(b = 0; b < buf[0]; b++)
						tarr[b] = calloc(buf[1] + 1, sizeof(char));
					unmarshal(buf, tarr);
					cc->phnum = buf[0];
					for(b = 0; b < buf[0]; b++)
						strncpy(cc->ph[b], tarr[b], buf[1] + 1);
				}

				if(strcmp("em", sqlite3_column_name(stmt, a)) == 0) {
					strncpy(buf, (char*)sqlite3_column_text(stmt, a), BBCH);
					for(b = 0; b < buf[0]; b++)
						tarr[b] = calloc(buf[1] + 1, sizeof(char));
					unmarshal(buf, tarr);
					cc->emnum = buf[0];
					for(b = 0; b < buf[0]; b++)
						strncpy(cc->em[b], tarr[b], buf[1] + 1);
				}

			}
			printcard(cc); // TODO: Replace with card deck builder
		}
	}

	return cc;
}

int main(int argc, char *argv[]) {

	char *cmd = calloc(MBCH, sizeof(char));
	char *cop = calloc(MBCH, sizeof(char));

	sqlite3 *db;

	int op = 0;
	int dbok = 0;
	int verb = 0;

	int optc;

	strncpy(cmd, basename(argv[0]), MBCH);

	while((optc = getopt(argc, argv, "v")) != -1) {
		switch (optc) {

			case 'v':
				verb++;
				break;

			default:
				usage(cmd);
				break;
		}
	}

	if(argc < optind + 1) errno = ESRCH; // TODO: Do dbdump instead
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
			printf("Database %s created successfully\n", DBNAME);
			return 0;
			}
	}

	card *cc = calloc(1, sizeof(card)); // TODO: Deck of cards?

	if(op == help) usage(cmd);
	else if(op == import) {
		FILE *f = fopen(argv[(optind + 1)], "r");
		if(f == NULL) {
			errno = ENOENT;
			usage(cmd);
		} else {
			icard(cc, f);
			dbok = wrdb(db, cc, verb);
			printf("dbok: %d\n", dbok);
		}
	}

	// if(op == export) ecard();
	if(op == phone) searchdb(db, cc, argv[(optind + 1)], verb);
	// if(op == email) searchdb();

	sqlite3_close(db);
	return 0;
}
