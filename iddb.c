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

int usage(const char *cmd) {

	if (errno) printf("Error: %s\n", strerror(errno));

	printf("%s %s - usage:\n", cmd, VER);
	printf("%s [args] command file/string\n", cmd);
	printf("Possible commands are:\n");
	printf("c[reate], i[mport], e[xport], h[elp], p[hone], m[ail], a[ll]\n");

	exit(errno);
}

// Validate card
int valcard(card *c) {
	//TODO: Make useful
	if(c->lid > 0) return 1;
	else return 0;
}

// Check for valid operation
static int chops(const char *cop) {

	char vops[7][7] = {"create", "import", "export", "help",
		"phone", "mail", "all"};

	int opnum = sizeof(vops) / sizeof(vops[0]);

	unsigned int a;

	for(a = 0; a < opnum; a++) {
		if(strlen(cop) == 1) { if(cop[0] == vops[a][0]) return a; }
		else { if(!strcmp(cop, vops[a])) return a; }
	}

	errno = EINVAL;
	return -1;
}

// Format string based on object key
static char *robj(char *buf, const char *key) {

	buf = strtok(buf, ":");
	buf = strtok(NULL, "\n");

	int p = strlen(buf) - 1;
	while(!isalnum(buf[p])) buf[p--] = '\0';

	return buf;
}

// Fetch index size
static int getindex(sqlite3 *db, const int verb) {

	int ret = 0;
	unsigned int a = 0;

	char *sql = calloc(BBCH, sizeof(char));
	sqlite3_stmt *stmt;

	snprintf(sql, BBCH, "SELECT * FROM id;");

	int dbop = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	if (verb > 1) printf("Query: %s\n", sql);

	// TODO: Replace with callback?
	while((dbop = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(dbop == SQLITE_ROW) {
			int ccnt = sqlite3_column_count(stmt);
			for(a = 0; a < ccnt; a++){
			if(!strcmp("lid", sqlite3_column_name(stmt, a))) 
				ret = atoi((char*)sqlite3_column_text(stmt, a));
			}
		}
	}

	free(sql);
	if(ret > -1) return ret;
	else return -1;
}

// Export card to file
static char *ecard(card *c, char *fpath, int psz, int verb) {

	char *rstr = calloc(EXPLEN + 1, sizeof(char));
	unsigned int a = 0;
	
	memset(fpath, 0, psz);

	randstr(rstr, EXPLEN);
	snprintf(fpath, MBCH, "%s%s%s", EXPDIR, rstr, EXPSUF);

	FILE *f = fopen(fpath, "w");

	fprintf(f, "BEGIN:VCARD\nVERSION:3.0\n");
	fprintf(f, "FN:%s\n", c->fn);
	if(c->uid[0]) fprintf(f, "UID:%s\n", c->uid);
	if(c->org[0]) fprintf(f, "ORG:%s\n", c->org);
	for(a = 0; a < c->emnum; a++) fprintf(f, "EMAIL;TYPE=HOME:%s\n", c->em[a]);
	for(a = 0; a < c->phnum; a++) fprintf(f, "TEL;TYPE=HOME:%s\n", c->ph[a]);
	fprintf(f, "END:VCARD\n");

	fclose(f);
	free(rstr);

	return fpath;
}

// Import card to struct
static card *icard(card *cc, FILE *f, sqlite3 *db, const int verb) {

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

	free(sql);
	return dbok;
}

// Print card to stdout
int printcard(card *cc, const int op, const int prnum, const int verb) {

	unsigned int a = 0;

	if(!cc->lid) return 1;

	if(op == all) {
		if(verb) printf("%d: ", cc->lid);
		printf("%s ", cc->fn);
		if(cc->org[0]) printf("(%s)\n", cc->org);
		else printf("\n");
		if(cc->uid[0] && verb) printf("UID: %s\n", cc->uid);

		for(a = 0; a < cc->emnum && a < prnum; a++) 
			printf("email %d: %s\n", a, cc->em[a]);
		for(a = 0; a < cc->phnum && a < prnum; a++)
			printf("phone %d: %s\n", a, cc->ph[a]);
		printf("\n");

	} else if(op == mail) {
		if(verb) printf("%s\n", cc->fn);
		for(a = 0; a < cc->emnum && a < prnum; a++) {
			if(verb) printf("email %d: ", a);
			printf("%s\n", cc->em[a]);
		}
	} else if(op == phone) {
		if(verb) printf("%s\n", cc->fn);
		for(a = 0; a < cc->phnum && a < prnum; a++) {
			if(verb) printf("phone %d: ", a);
			printf("%s\n", cc->ph[a]);
		}
	}

	return 0;
}

// Write counter to index
static int wrindex(sqlite3 *db, const int i, const int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	char *err = 0;

	snprintf(sql, BBCH, "UPDATE ctr SET num = %d;", i);
	int ret = sqlite3_exec(db, sql, 0, 0, &err);

	if(verb > 1) printf("Query: %s\n", sql);

	free(sql);
	return ret;
}

// Write struct to DB
int wrdb(sqlite3 *db, card *cc, const int op, const int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	char *pbuf = calloc(PHNUM * PHLEN, sizeof(char));
	char *mbuf = calloc(EMNUM * EMLEN, sizeof(char));

	char *err = 0;

	if(verb) printcard(cc, op, NUMCARD, verb);

	snprintf(sql, BBCH, "INSERT INTO id VALUES"
			"(%d, '%s', '%s', '%s',  '%s', '%s');",
			++cc->lid, cc->uid, cc->fn, cc->org,
			marshal(pbuf, cc->phnum, PHLEN, cc->ph),
			marshal(mbuf, cc->emnum, EMLEN, cc->em));

	if (verb > 1) printf("Query: %s\n", sql);

	int dbok = sqlite3_exec(db, sql, 0, 0, &err);
	if(!dbok) wrindex(db, cc->lid, verb);

	free(sql);
	free(pbuf);
	free(mbuf);
	return dbok;
}

// Return 0 if c1 and c2 are identical (TODO: implement)
int cmpcard(const card *c1, const card *c2) {

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

	if(!strcmp("lid", cn)) cc->lid = atoi(ct);
	if(!strcmp("uid", cn)) strcpy(cc->uid, ct);
	if(!strcmp("fn", cn)) strcpy(cc->fn, ct);
	if(!strcmp("org", cn)) strcpy(cc->org, ct);

	if(!strcmp("ph", cn)) {
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

	if(!strcmp("em", cn)) {
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

// Return entry from database (TODO: Separate to deck and single)
static card **searchdb(sqlite3 *db, card **cc, char *str, int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	unsigned int cci = 0;

	sqlite3_stmt *stmt;

	if(str == NULL) snprintf(sql, BBCH, "SELECT * FROM id;");
	else snprintf(sql, BBCH, "SELECT * FROM id WHERE fn LIKE '%%%s%%';", str);

	if (verb > 1) printf("Query: %s\n", sql);

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
	int prnum = NUMCARD;

	unsigned int a = 0;

	int optc;

	strncpy(cmd, basename(argv[0]), MBCH);

	while((optc = getopt(argc, argv, "n:v")) != -1) {
		switch (optc) {

			case 'n':
				prnum = atoi(optarg);
				if(prnum > NUMCARD) prnum = NUMCARD;
				break;

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
	if(op < 0) errno = EINVAL;
	if(errno) usage(cmd);

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
	for(a = 0; a < NUMCARD; a++) {
		cc[a] = calloc(1, sizeof(card));
		cc[a]->lid = -1;
	}

	// Initiate operations TODO: create new func()
	if(op == help) {
		usage(cmd);

	} else if(op == import) {
		FILE *f = fopen(argv[(optind + 1)], "r");
		if(f == NULL) {
			errno = ENOENT;
			usage(cmd);
		} 
		icard(cc[0], f, db, verb);
		dbok = wrdb(db, cc[0], op, verb);
		if(dbok) printf("SQL Error #: %d\n", dbok);
		fclose(f);

	} else if(op == export) { 
		searchdb(db, cc, argv[(optind + 1)], verb);
		char *fpath = calloc(MBCH, sizeof(char));
		for(a = 0; a < prnum; a++) {
			if(valcard(cc[a])) {
				ecard(cc[a], fpath, MBCH, verb);
				if(verb) printf("%s exported as: %s\n", cc[a]->fn, fpath);
			} else break;
		}
		free(fpath);

	} else if(op == phone || op == mail || op == all) {
		searchdb(db, cc, argv[(optind + 1)], verb);
		for(a = 0; a < NUMCARD; a++)  {
			if(valcard(cc[a])) printcard(cc[a], op, prnum, verb);
			else break;
		}
	}

	free(cmd);
	free(cop);
	sqlite3_close(db);
	return 0;
}
