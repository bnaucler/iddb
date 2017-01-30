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
static card *icard(card *c, FILE *f, sqlite3 *db, const int verb) {

	char *buf = malloc(MBCH);
	int verc = 0;

	c->phnum = 0;
	c->emnum = 0;

	while(fgets(buf, MBCH, f)){
		if(!strst(buf, STARTKEY)) verc++;
		else if(!strst(buf, STOPKEY)) verc++;
		else if(!strst(buf, UIDKEY))
			strncpy(c->uid, robj(buf, UIDKEY), ULEN);
		else if(!strst(buf, FNKEY))
			strncpy(c->fn, robj(buf, FNKEY), NALEN);
		else if(!strst(buf, ORGKEY))
			strncpy(c->org, robj(buf, ORGKEY), ORLEN);
		else if(!strst(buf, EMKEY))
			strncpy(c->em[c->emnum++], robj(buf, EMKEY), EMLEN);
		else if(!strst(buf, PHKEY))
			strncpy(c->ph[c->phnum++], robj(buf, PHKEY), PHLEN);
	}

	if(verc == 2) c->lid = getindex(db, verb);

	free(buf);
	return c;
}

// Create (or reset) database table
static int ctable(sqlite3 *db) {

	char *sql = calloc(DBCH, sizeof(char));
	char *err = 0;
	int dbrc = 0;

	strncpy(sql, "DROP TABLE IF EXISTS id;"
		"CREATE TABLE id(lid INT, uid TEXT, fn TEXT,"
		" org TEXT, ph TEXT, em TEXT);", DBCH);
	dbrc = sqlite3_exec(db, sql, 0, 0, &err);

	if(!dbrc) {
		strncpy(sql, "DROP TABLE IF EXISTS ctr;"
			"CREATE TABLE ctr(addr INT, num INT);", DBCH);
		dbrc = sqlite3_exec(db, sql, 0, 0, &err);
		if(!dbrc) strncpy(sql, "INSERT INTO ctr VALUES(0, 0);", BBCH);
		dbrc = sqlite3_exec(db, sql, 0, 0, &err);
	}

	free(sql);
	return dbrc;
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
int wrcard(sqlite3 *db, card *c, const int op, const int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	char *pbuf = calloc(PHNUM * PHLEN, sizeof(char));
	char *mbuf = calloc(EMNUM * EMLEN, sizeof(char));

	char *err = 0;

	snprintf(sql, BBCH, "INSERT INTO id VALUES"
			"(%d, '%s', '%s', '%s',  '%s', '%s');",
			++c->lid, c->uid, c->fn, c->org,
			marshal(pbuf, c->phnum, PHLEN, c->ph),
			marshal(mbuf, c->emnum, EMLEN, c->em));

	if (verb > 1) printf("Query: %s\n", sql);

	int dbrc = sqlite3_exec(db, sql, 0, 0, &err);
	if(!dbrc) wrindex(db, c->lid, verb);

	free(sql);
	free(pbuf);
	free(mbuf);
	return dbrc;
}

// Read SQL data into card
static card *readid(card *c, const char *cn, const char *ct) {

	char *buf = calloc(BBCH, sizeof(char));
	unsigned int a = 0;

	if(!strcmp("lid", cn)) c->lid = matoi(ct);
	if(!strcmp("uid", cn)) strcpy(c->uid, ct);
	if(!strcmp("fn", cn)) strcpy(c->fn, ct);
	if(!strcmp("org", cn)) strcpy(c->org, ct);

	if(!strcmp("ph", cn)) {
		char **parr = calloc(PHNUM * PHLEN, sizeof(char));
		strncpy(buf, ct, BBCH);
		for(a = 0; a < buf[0]; a++)
			parr[a] = calloc(buf[1] + 1, sizeof(char));
		unmarshal(buf, parr);
		c->phnum = buf[0];
		for(a = 0; a < buf[0]; a++)
			strncpy(c->ph[a], parr[a], buf[1] + 1);
		free(parr);
	}

	if(!strcmp("em", cn)) {
		char **earr = calloc(EMNUM * EMLEN, sizeof(char));
		strncpy(buf, ct, BBCH);
		for(a = 0; a < buf[0]; a++)
			earr[a] = calloc(buf[1] + 1, sizeof(char));
		unmarshal(buf, earr);
		c->emnum = buf[0];
		for(a = 0; a < buf[0]; a++)
			strncpy(c->em[a], earr[a], buf[1] + 1);
		free(earr);
	}

	free(buf);
	return c;
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

// Return entry from database 
static int searchdb(sqlite3 *db, card **cc, char *sql, 
		unsigned int cci, int mxnum, int verb) {

	card *tmpc = calloc(1, sizeof(card));
	sqlite3_stmt *stmt;

	if (verb > 1) printf("Query: %s\n", sql);

	int dbrc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	unsigned int a = 0, isdbl = 0;

	if (dbrc && verb) fprintf(stderr, "SQL error: %d\n", dbrc);

	while((dbrc = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(dbrc == SQLITE_ROW) {
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
static int mkdeck(sqlite3 *db, card **cc, char *str, int svar, 
		int mxnum, int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	unsigned int a = 0, cci = 0;

	if(!str) {
		snprintf(sql, BBCH, "SELECT * FROM id;");
		cci = searchdb(db, cc, sql, cci, mxnum, verb);

	} else if (svar < 0) {
		for(a = 0; a < 6; a++) {
			if(cci >= mxnum) return cci;
			mksqlstr(a, sql, str);
			cci = searchdb(db, cc, sql, cci, mxnum, verb);
		}
	
	} else {
		mksqlstr(svar, sql, str);
		cci = searchdb(db, cc, sql, cci, mxnum, verb);
	}

	free(sql);
	return cci;
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

// Deletes card #lid from the database
static int delcard(sqlite3 *db, int lid, int verb) {

	int last = getindex(db, verb);
	char *sql = calloc(BBCH, sizeof(char));
	char *err = 0;

	snprintf(sql, BBCH, "DELETE FROM id WHERE lid=%d;", lid);
	if(last != lid) { if(mvcard(db, lid, last)) return -1; }
	wrindex(db, --last, verb);

	int dbrc = sqlite3_exec(db, sql, 0, 0, &err);

	free(sql);
	return dbrc;
}

// Execute create operation
static int exec_create(sqlite3 *db, const char *cmd) {

	if (ctable(db)) {
		errno = EEXIST; // TODO: Better error messages..
		return usage(cmd);

	} else {
		printf("Database %s created successfully\n", DBNAME);
		return 0;
	}
}

// Execute import operation
static int exec_import(sqlite3 *db, char **args, const char *cmd,
	const int op, const int numf, const int mxnum, const int verb) {

	int dbrc = 0;
	unsigned int a = 0;

	card **cc = dalloc(numf, sizeof(card));

	for(a = 0; a < numf; a++) {
		DIR *d = opendir(args[a]);
		if(d && !errno) {
			// TODO: Dirent walk

		} else {
			FILE *f = fopen(args[a], "r");
			if(f == NULL) {
				errno = ENOENT;
				return usage(cmd);
			} 

			icard(cc[a], f, db, verb);
			dbrc = wrcard(db, cc[a], op, verb);
			if(dbrc) printf("SQL Error #: %d\n", dbrc);
			else if(verb) printcard(cc[0], op, mxnum, verb);
			fclose(f);
		}
	}

	free(cc);
	return 0;
}

// Execute export operation
static int exec_export(sqlite3 *db, char **args, const int numarg,
	const int svar, const int mxnum, const int verb) {

	unsigned int a = 0, cci = 0;

	char *sstr = calloc(BBCH, sizeof(char));
	char *fpath = calloc(MBCH, sizeof(char));

	card **cc = dalloc(mxnum, sizeof(card));
	strncpy(sstr, atostr(sstr, args, numarg), BBCH);

	setsrand();

	cci = mkdeck(db, cc, sstr, svar, mxnum, verb);
	for(a = 0; a < cci; a++) {
		ecard(cc[a], fpath, MBCH, verb);
		if(verb) printf("%s exported as: %s\n", cc[a]->fn, fpath);
	}

	free(sstr);
	free(fpath);
	free(cc);
	return 0;
}

// Execute 'new' operation TODO: Default values from args
static int exec_new(sqlite3 *db, const char *cmd, const int op,
	const int mxnum, const int verb) {

	int dbrc = 0;
	card *c = calloc(1, sizeof(card));

	setsrand();
	mknew(db, c, verb);

	if(valcard(c)) {
		dbrc = wrcard(db, c, op, verb);
		if(dbrc) printf("SQL Error #: %d\n", dbrc);
		else if(verb) printcard(c, op, mxnum, verb);

	} else {
		errno = EINVAL;
		return usage(cmd);
	}

	free(c);
	return 0;
}

// Execute delete operation
static int exec_delete(sqlite3 *db, char **args, const int numarg,
	const int op, const int mxnum, const int verb) {

	int svar = lid;

	card **cc = dalloc(1, sizeof(card));

	mkdeck(db, cc, args[0], svar, mxnum, verb);
	if(valcard(cc[0])) {
		delcard(db, cc[0]->lid, verb);
		if(verb) {
			printf("Deleted card:\n");
			printcard(cc[0], op, mxnum, 2);
		}
	} else {
		printf("Found no contact with ID #%d.\n", matoi(args[0]));
	}

	free(cc);
	return 0;
}
 
// Execute search operations (all, phone & mail)
static int exec_search(sqlite3 *db, char **args, const int numarg,
	const int op, const int svar, const int mxnum, const int verb) {

	unsigned int a = 0, cci = 0;

	char *sstr = calloc(BBCH, sizeof(char));
	card **cc = dalloc(mxnum, sizeof(card));

	strncpy(sstr, atostr(sstr, args, numarg), BBCH);

	cci = mkdeck(db, cc, sstr, svar, mxnum, verb);
	for(a = 0; a < cci; a++) printcard(cc[a], op, mxnum, verb);

	free(sstr);
	free(cc);
	return 0;
}

// Execute operations
static int execute(sqlite3 *db, const int op, int svar, const char *cmd, 
	const int mxnum, const int alen, char **args, const int verb) {

	errno = 0;

	switch(op) {

		case help:
			return usage(cmd);

		case create:
			return exec_create(db, cmd);

		case import:
			return exec_import(db, args, cmd, op, alen, mxnum, verb);

		case export:
			return exec_export(db, args, alen, svar, mxnum, verb);

		case new:
			return exec_new(db, cmd, op, mxnum, verb);

		case delete:
			return exec_delete(db, args, alen, op, mxnum, verb);

		default:
			return exec_search(db, args, alen, op, svar, mxnum, verb);
	}
}

int main(int argc, char **argv) {

	char *cmd = calloc(MBCH, sizeof(char));

	sqlite3 *db;

	int op = 0, dbrc = 0, verb = 0, ret = 0;
	int mxnum = NUMCARD;
	int svar = -1; // TODO: implement via getopt
	int optc;

	strncpy(cmd, basename(argv[0]), MBCH);

	while((optc = getopt(argc, argv, "hn:v")) != -1) {
		switch(optc) {

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

	// Break if no argument has been given
	if(argc < optind + 1) errno = ESRCH; // TODO: Do dbdump instead
	if(errno) return usage(cmd);

	// Set and verify operation
	op = chops(argv[optind]);
	if(op < 0) errno = EINVAL;
	if(errno) return usage(cmd);

	// Open database
	dbrc = sqlite3_open(DBNAME, &db);
	if(dbrc) errno = ENOENT;
	if(errno) return usage(cmd);

	// Prepare argument list
	argc -= optind + 1;
	argv += optind + 1;

	// Initiate operations
	ret = execute(db, op, svar, cmd, mxnum, argc, argv, verb);

	// Graceful exit
	free(cmd);
	sqlite3_close(db);
	return ret;
}
