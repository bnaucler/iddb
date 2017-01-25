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
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sqlite3.h>

int usage(const char *cmd) {

	if (errno) printf("Error: %s\n", strerror(errno));

	printf("%s %s - usage: ", cmd, VER);
	printf("%s [args] command file/string\n", cmd);
	printf("Available commands:\n");
	printf("c[reate], d[elete], i[mport], e[xport], h[elp]\n" 
			"p[hone], m[ail], n[ew], a[ll]\n");

	return errno;
}

// Validate card
int valcard(card *c) {
	//TODO: Make useful
	if(c->lid > 0) return 1;
	else return 0;
}

// Check for valid operation
static int chops(const char *cop) {

	char vops[9][7] = {"create", "delete", "import", "export", "help",
		"phone", "mail", "new", "all"};

	int opnum = sizeof(vops) / sizeof(vops[0]);

	unsigned int a;

	for(a = 0; a < opnum; a++) {
		if(strlen(cop) == 1) { if(cop[0] == vops[a][0]) return a; }
		else { if(!strcmp(cop, vops[a])) return a; }
	}

	errno = EINVAL;
	return -1;
}

// Fetch index size
static int getindex(sqlite3 *db, const int verb) {

	int ret = 0;
	unsigned int a = 0;

	char *sql = calloc(BBCH, sizeof(char));
	sqlite3_stmt *stmt;

	snprintf(sql, BBCH, "SELECT * FROM ctr;");

	int dbop = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	if (verb > 1) printf("Query: %s\n", sql);

	// TODO: Replace with callback?
	while((dbop = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(dbop == SQLITE_ROW) {
			int ccnt = sqlite3_column_count(stmt);
			for(a = 0; a < ccnt; a++){
			if(!strcmp("num", sqlite3_column_name(stmt, a))) 
				ret = matoi((char*)sqlite3_column_text(stmt, a));
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
	for(a = 0; a < c->emnum; a++) fprintf(f, "EMAIL;TYPE=INTERNET:%s\n", c->em[a]);
	for(a = 0; a < c->phnum; a++) fprintf(f, "TEL;TYPE=VOICE:%s\n", c->ph[a]);
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
int printcard(card *cc, const int op, const int mxnum, const int verb) {

	unsigned int a = 0;

	if(!cc->lid) return 1;

	if(op == all || op == new || op == delete) {
		if(verb) printf("%d: ", cc->lid);
		printf("%s ", cc->fn);
		if(cc->org[0]) printf("(%s)\n", cc->org);
		else printf("\n");
		if(cc->uid[0] && verb) printf("UID: %s\n", cc->uid);

		for(a = 0; a < cc->emnum && a < mxnum; a++) 
			printf("email %d: %s\n", a, cc->em[a]);
		for(a = 0; a < cc->phnum && a < mxnum; a++)
			printf("phone %d: %s\n", a, cc->ph[a]);
		printf("\n");

	} else if(op == mail) {
		if(verb) printf("%d: %s\n", cc->lid, cc->fn);
		for(a = 0; a < cc->emnum && a < mxnum; a++) {
			if(verb) printf("email %d: ", a);
			printf("%s\n", cc->em[a]);
		}
	} else if(op == phone) {
		if(verb) printf("%d: %s\n", cc->lid, cc->fn);
		for(a = 0; a < cc->phnum && a < mxnum; a++) {
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

	if(strncmp(c1->uid, c2->uid, ULEN)) return 1;
	if(strncmp(c1->fn, c2->fn, NALEN)) return 2;
	if(strncmp(c1->org, c2->org, ORLEN)) return 2;
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

	if(!strcmp("lid", cn)) cc->lid = matoi(ct);
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

// Create SQL search string
static int mksqlstr(int svar, char *sql, char *str) {

	switch(svar) {
		case lid:
		snprintf(sql, BBCH, "SELECT * FROM id WHERE lid=%d;", matoi(str));
		break;

		case uid:
		snprintf(sql, BBCH, "SELECT * FROM id WHERE uid LIKE '%%%s%%';", str);
		break;

		case fn:
		snprintf(sql, BBCH, "SELECT * FROM id WHERE fn LIKE '%%%s%%';", str);
		break;

		case org:
		snprintf(sql, BBCH, "SELECT * FROM id WHERE org LIKE '%%%s%%';", str);
		break;

		case ph:
		snprintf(sql, BBCH, "SELECT * FROM id WHERE ph LIKE '%%%s%%';", str);
		break;

		case em:
		snprintf(sql, BBCH, "SELECT * FROM id WHERE em LIKE '%%%s%%';", str);
		break;

	}

	return 0;
}

// Copy card by value (TODO: error checking)
static int cpcard(card *dc, const card *sc) {

	unsigned int a = 0;

	if(sc->lid > 0) dc->lid = sc->lid;
	if(sc->uid[0]) strncpy(dc->uid, sc->uid, ULEN);
	strncpy(dc->fn, sc->fn, NALEN);
	dc->phnum = sc->phnum;
	dc->emnum = sc->emnum;
	if(sc->org[0]) strncpy(dc->org, sc->org, ORLEN);

	for(a = 0; a < sc->phnum; a++) strncpy(dc->ph[a], sc->ph[a], PHLEN);
	for(a = 0; a < sc->emnum; a++) strncpy(dc->em[a], sc->em[a], EMLEN);

	return 0;
}

// Return entry from database 
static int searchdb(sqlite3 *db, card **cc, char *sql, 
		unsigned int cci, int mxnum, int verb) {

	card *tmpc = calloc(1, sizeof(card));
	sqlite3_stmt *stmt;

	if (verb > 1) printf("Query: %s\n", sql);

	int dbok = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	unsigned int a = 0, isdbl = 0;

	if (dbok && verb) fprintf(stderr, "SQL error: %d\n", dbok);

	while((dbok = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(dbok == SQLITE_ROW) {
			int ccnt = sqlite3_column_count(stmt);
			for(a = 0; a < ccnt; a++){
				readid(tmpc, sqlite3_column_name(stmt, a),
						(char*)sqlite3_column_text(stmt, a));
			}
		}

		for(a = 0; a < cci; a++) { if(!cmpcard(tmpc, cc[a])) isdbl++; }

		if(!isdbl) {
			cpcard(cc[cci++], tmpc);
			if(cci >= mxnum) return cci;
		}
		isdbl = 0;
	}

	free(tmpc);
	return cci;
}

// Create deck of cards - init search(es)
static card **mkdeck(sqlite3 *db, card **cc, char *str, int svar, 
		int mxnum, int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	unsigned int a = 0, cci = 0;

	if(!str) {
		snprintf(sql, BBCH, "SELECT * FROM id;");
		searchdb(db, cc, sql, cci, mxnum, verb);
		return cc;

	} else if (svar < 0) {
		for(a = 0; a < 6; a++) {
			if(cci >= mxnum) return cc;
			mksqlstr(a, sql, str);
			cci = searchdb(db, cc, sql, cci, mxnum, verb);
		}
	
	} else {
		mksqlstr(svar, sql, str);
		searchdb(db, cc, sql, cci, mxnum, verb);
		return cc;
	}

	free(sql);
	return cc;
}

// Read new card from stdin
static int mknew(sqlite3 *db, card *c, const int verb) {

	char *buf = calloc(MBCH, sizeof(char));
	char *pstr = calloc(SBCH, sizeof(char));

	unsigned int a = 0;

	if(readline("Full name", buf, NALEN)) return 1;
	strcpy(c->fn, buf);

	if(randstr(buf, UCLEN)) strcpy(c->uid, buf);
	c->lid = getindex(db, verb);

	if(!readline("Organization", buf, ORLEN)) strcpy(c->org, buf);

	for(a = 0; a < PHNUM; a++) {
		snprintf(pstr, SBCH, "Phone %d", a);
		if(readline(pstr, buf, PHLEN)) break;
		strcpy(c->ph[a], buf);
	}
	c->phnum = a;

	for(a = 0; a < EMNUM; a++) {
		snprintf(pstr, SBCH, "Email %d", a);
		if(readline(pstr, buf, EMLEN)) break;
		strcpy(c->em[a], buf);
	}
	c->emnum = a;

	free(buf);
	free(pstr);

	return 0;
}

// Change card LID from plid to nlid
static int mvcard(sqlite3 *db, int plid, int nlid) {

	char *sql = calloc(BBCH, sizeof(char));
	char *err = 0;

	snprintf(sql, BBCH, "UPDATE id SET lid=%d WHERE lid=%d;", nlid, plid);

	int dbok = sqlite3_exec(db, sql, 0, 0, &err);
	free(sql);
	return dbok;
}

// Deletes card #lid from the database
static int delcard(sqlite3 *db, int lid, int verb) {

	int last = getindex(db, verb);
	char *sql = calloc(BBCH, sizeof(char));
	char *err = 0;

	snprintf(sql, BBCH, "DELETE FROM id WHERE lid=%d;", lid);
	if(last != lid) { if(mvcard(db, lid, last)) return -1; }
	wrindex(db, --last, verb);

	int dbok = sqlite3_exec(db, sql, 0, 0, &err);
	free(sql);
	return dbok;
}

// Allocate memory for a deck of num cards
static card **deckalloc(int num, int sz) {

	card **cc = calloc(num, sz);
	unsigned int a = 0;

	for(a = 0; a < num; a++) {
		cc[a] = calloc(1, sz);
		cc[a]->lid = -1;
	}
	return cc;
}

int main(int argc, char *argv[]) {

	char *cmd = calloc(MBCH, sizeof(char));
	char *cop = calloc(MBCH, sizeof(char));
	card **cc;

	sqlite3 *db;

	int op = 0, dbok = 0, verb = 0;
	int mxnum = NUMCARD;

	int svar = -1;

	unsigned int a = 0;

	int optc;

	strncpy(cmd, basename(argv[0]), MBCH);

	while((optc = getopt(argc, argv, "hn:v")) != -1) {
		switch (optc) {

			case 'h':
				return usage(cmd);
				break;

			case 'n':
				mxnum = matoi(optarg);
				if(mxnum > NUMCARD) mxnum = NUMCARD;
				break;

			case 'v':
				verb++;
				break;

			default:
				return usage(cmd);
				break;
		}
	}

	if(argc < optind + 1) errno = ESRCH; // TODO: Do dbdump instead
	if(errno) return usage(cmd);

	strncpy(cop, argv[optind], MBCH);
	op = chops(cop);
	if(op < 0) errno = EINVAL;
	if(errno) return usage(cmd);

	dbok = sqlite3_open(DBNAME, &db);
	if(dbok) errno = ENOENT;
	// if(errno) return usage(cmd);

	if(op == create) {
		if (ctable(db)) {
			errno = EEXIST; // TODO: Better error messages..
			return usage(cmd);
		} else {
			printf("Database %s created successfully\n", DBNAME);
			return 0;
			}
	}


	// Initiate operations TODO: create new func()
	if(op == help) {
		return usage(cmd);

	} else if(op == import) {
		errno = 0;
		DIR *d = opendir(argv[(optind + 1)]);
		if(d && !errno) {
			printf("DEBUG: BATCH IMPORT\n");
			exit(0);
		} else {
			cc = deckalloc(1, sizeof(card));
			FILE *f = fopen(argv[(optind + 1)], "r");
			if(f == NULL) {
				errno = ENOENT;
				return usage(cmd);
			} 
			icard(cc[0], f, db, verb);
			dbok = wrdb(db, cc[0], op, verb);
			if(dbok) printf("SQL Error #: %d\n", dbok);
			else if(verb) printcard(cc[0], op, mxnum, verb);
			fclose(f);
		}

	} else if(op == export) { 
		cc = deckalloc(mxnum, sizeof(card));
		mkdeck(db, cc, argv[(optind + 1)], svar, mxnum, verb);
		char *fpath = calloc(MBCH, sizeof(char));
		for(a = 0; a < mxnum; a++) {
			if(valcard(cc[a])) {
				ecard(cc[a], fpath, MBCH, verb);
				if(verb) printf("%s exported as: %s\n", cc[a]->fn, fpath);
			} else break;
		}
		free(fpath);

	} else if(op == phone || op == mail || op == all) {
		cc = deckalloc(mxnum, sizeof(card));
		mkdeck(db, cc, argv[(optind + 1)], svar, mxnum, verb);
		for(a = 0; a < mxnum; a++)  {
			if(valcard(cc[a])) printcard(cc[a], op, mxnum, verb);
			else break;
		}

	} else if(op == new) {
		cc = deckalloc(1, sizeof(card));
		// TODO: Default values from argv / optarg
		mknew(db, cc[0], verb);
		if(valcard(cc[0])) {
			dbok = wrdb(db, cc[0], op, verb);
			if(dbok) printf("SQL Error #: %d\n", dbok);
			else if(verb) printcard(cc[0], op, mxnum, verb);
		} else {
			errno = EINVAL;
			return usage(cmd);
		}
	} else if(op == delete) {
		cc = deckalloc(1, sizeof(card));
		svar = lid;
		mkdeck(db, cc, argv[(optind + 1)], svar, mxnum, verb);
		if(valcard(cc[0])) {
			dbok = delcard(db, cc[0]->lid, verb);
			if(verb) {
				printcard(cc[0], op, mxnum, 2);
				printf("Deleted card:\n");
			}
		} else {
			printf("Found no contact with ID #%d.\n", matoi(argv[(optind + 1)]));
		}
	}

	free(cc);
	free(cmd);
	free(cop);
	sqlite3_close(db);
	return 0;
}
