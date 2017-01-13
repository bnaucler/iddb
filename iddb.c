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

	char vops[6][7] = {"create", "import", "export", "help", "phone", "email"};

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
static int strst(char *str, char *key) {

	int klen = strlen(key);
	unsigned int a = 0;

	// Not sure if tolower is needed
	for(a = 0; a < klen; a++) {
		if(tolower(str[a]) != tolower(key[a])) return 1;
	}

	return 0;
}

// Format string based on object key
static char *robj(char *buf, char *key) {

	buf = strtok(buf, ":");
	buf = strtok(NULL, "\n");

	int p = strlen(buf) - 1;
	while(!isalnum(buf[p])) buf[p--] = '\0';

	return buf;
}

// Fetch index size
int getindex(sqlite3 *db, int verb) {

	int ret = 0;
	unsigned int a = 0;

	char *sql = calloc(BBCH, sizeof(char));
	sqlite3_stmt *stmt;

	snprintf(sql, BBCH, "SELECT * FROM id;");

	int dbop = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	if (verb) printf("Query: %s\n", sql);

	// TODO: Replace with callback?
	while((dbop = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(dbop == SQLITE_ROW) {
			int ccnt = sqlite3_column_count(stmt);
			for(a = 0; a < ccnt; a++){
			if(strcmp("lid", sqlite3_column_name(stmt, a)) == 0) 
				ret = atoi((char*)sqlite3_column_text(stmt, a));
			}
		}
	}

	if(ret > -1) return ret;
	else return -1;
}

// Import card to struct
static card *icard(card *cc, FILE *f, sqlite3 *db, int verb) {

	char *buf = malloc(MBCH);
	int verc = 0;

	cc->phnum = 0;
	cc->emnum = 0;

	while(fgets(buf, MBCH, f)){
		if(!strst(buf, STARTKEY)) verc++;
		else if(!strst(buf, STOPKEY)) verc++;
		else if(!strst(buf, UIDKEY))
			strncpy(cc->uid, robj(buf, UIDKEY), ULEN);
		else if(!strst(buf, FNKEY))
			strncpy(cc->fn, robj(buf, FNKEY), NALEN);
		else if(!strst(buf, ORGKEY))
			strncpy(cc->org, robj(buf, ORGKEY), ORLEN);
		else if(!strst(buf, EMKEY))
			strncpy(cc->em[cc->emnum++], robj(buf, EMKEY), EMLEN);
		else if(!strst(buf, PHKEY))
			strncpy(cc->ph[cc->phnum++], robj(buf, PHKEY), PHLEN);
	}

	if(verc == 2) cc->lid = getindex(db, verb);

	free(buf);
	return cc;
}

// Create (or reset) database table
static int ctable(sqlite3 *db) {

	char *sql = calloc(DBCH, sizeof(char));
	char *err = 0;
	int dbok = 0;

	strncpy(sql, "DROP TABLE IF EXISTS id;"
		"CREATE TABLE id(lid INT, uid TEXT, fn TEXT,"
		" org TEXT, ph TEXT, em TEXT);", DBCH);
	dbok = sqlite3_exec(db, sql, 0, 0, &err);

	if(!dbok) {
		strncpy(sql, "DROP TABLE IF EXISTS ctr;"
			"CREATE TABLE ctr(addr INT, num INT);", DBCH);
		dbok = sqlite3_exec(db, sql, 0, 0, &err);
		if(!dbok) strncpy(sql, "INSERT INTO ctr VALUES(0, 0);", BBCH);
		dbok = sqlite3_exec(db, sql, 0, 0, &err);
	}

	return dbok;
}

// Print card to stdout
void printcard(card *cc, int verb) {

	unsigned int a = 0;

	if(!cc->lid) return;
	if(cc->uid[0] && verb) printf("uid: %s\n", cc->uid);
	printf("fn: %s\n", cc->fn);
	if(cc->org[0]) printf("org: %s\n", cc->org);

	for(a = 0; a < cc->emnum; a++) printf("em %d: %s\n", a, cc->em[a]);
	for(a = 0; a < cc->phnum; a++) printf("ph %d: %s\n", a, cc->ph[a]);
}

// Write counter to index
int wrindex(sqlite3 *db, int i, int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	char *err = 0;

	snprintf(sql, BBCH, "UPDATE ctr SET num = %d;", i);
	int ret = sqlite3_exec(db, sql, 0, 0, &err);

	if(verb) printf("Query: %s\n", sql);

	return ret;
}

// Write struct to DB
int wrdb(sqlite3 *db, card *cc, int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	char *pbuf = calloc(PHNUM * PHLEN, sizeof(char));
	char *mbuf = calloc(EMNUM * EMLEN, sizeof(char));

	char *err = 0;

	if(verb) printcard(cc, verb);

	snprintf(sql, BBCH, "INSERT INTO id VALUES"
			"(%d, '%s', '%s', '%s',  '%s', '%s');",
			++cc->lid, cc->uid, cc->fn, cc->org,
			marshal(pbuf, cc->phnum, PHLEN, cc->ph),
			marshal(mbuf, cc->emnum, EMLEN, cc->em));

	if (verb) printf("Query: %s\n", sql);

	int dbok = sqlite3_exec(db, sql, 0, 0, &err);
	if(!dbok) wrindex(db, cc->lid, verb);
	return dbok;
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

// Read SQL data into card
static card *readid(card *cc, const char *cn, const char *ct) {

	char *buf = calloc(BBCH, sizeof(char));

	unsigned int a = 0;

	if(strcmp("lid", cn) == 0) cc->lid = atoi(ct);
	if(strcmp("uid", cn) == 0) strcpy(cc->uid, ct);
	if(strcmp("fn", cn) == 0) strcpy(cc->fn, ct);
	if(strcmp("org", cn) == 0) strcpy(cc->org, ct);

	if(strcmp("ph", cn) == 0) {
		char **parr = calloc(PHNUM * PHLEN, sizeof(char));
		strncpy(buf, ct, BBCH);
		for(a = 0; a < buf[0]; a++)
			parr[a] = calloc(buf[1] + 1, sizeof(char));
		unmarshal(buf, parr);
		cc->phnum = buf[0];
		for(a = 0; a < buf[0]; a++)
			strncpy(cc->ph[a], parr[a], buf[1] + 1);
		free(parr);
	}

	if(strcmp("em", cn) == 0) {
		char **earr = calloc(EMNUM * EMLEN, sizeof(char));
		strncpy(buf, ct, BBCH);
		for(a = 0; a < buf[0]; a++)
			earr[a] = calloc(buf[1] + 1, sizeof(char));
		unmarshal(buf, earr);
		cc->emnum = buf[0];
		for(a = 0; a < buf[0]; a++)
			strncpy(cc->em[a], earr[a], buf[1] + 1);
		free(earr);
	}

	free(buf);
	return cc;
}
// Return entry from database
static card **searchdb(sqlite3 *db, card **cc, char *str, int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	unsigned int cci = 0;

	sqlite3_stmt *stmt;

	if(str == NULL) snprintf(sql, BBCH, "SELECT * FROM id;");
	else snprintf(sql, BBCH, "SELECT * FROM id WHERE fn LIKE '%%%s%%';", str);

	if (verb) printf("Query: %s\n", sql);

	int dbok = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	unsigned int a = 0;

	if (dbok && verb) printf("SQL error: %d\n", dbok);

	while((dbok = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(dbok == SQLITE_ROW) {
			int ccnt = sqlite3_column_count(stmt);
			for(a = 0; a < ccnt; a++){
				readid(cc[cci], sqlite3_column_name(stmt, a),
						(char*)sqlite3_column_text(stmt, a));
			}
			if(cci++ > NUMCARD) return cc;
		}
	}

	free(sql);
	return cc;
}

int main(int argc, char *argv[]) {

	char *cmd = calloc(MBCH, sizeof(char));
	char *cop = calloc(MBCH, sizeof(char));

	sqlite3 *db;

	int op = 0;
	int dbok = 0;
	int verb = 0;

	unsigned int a = 0;

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
			errno = EEXIST; // TODO: Better error messages..
			usage(cmd);
		} else {
			printf("Database %s created successfully\n", DBNAME);
			return 0;
			}
	}

	// Create a deck of cards
	card **cc = calloc(NUMCARD, sizeof(card));
	for(a = 0; a < NUMCARD; a++) cc[a] = calloc(1, sizeof(card));

	if(op == help) usage(cmd);
	else if(op == import) {
		FILE *f = fopen(argv[(optind + 1)], "r");
		if(f == NULL) {
			errno = ENOENT;
			usage(cmd);
		} else {
			icard(cc[0], f, db, verb);
			dbok = wrdb(db, cc[0], verb);
			if(dbok) printf("SQL Error #: %d\n", dbok);
		}
	}

	// if(op == export) ecard();
	if(op == phone) {
		searchdb(db, cc, argv[(optind + 1)], verb);
		for(a = 0; a < NUMCARD; a++) printcard(cc[a], verb);
	}
	// if(op == email) searchdb();

	sqlite3_close(db);
	return 0;
}
