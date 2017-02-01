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

// Check for valid operation TODO: case insensitivity
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
			esccpy(c->uid, robj(buf, UIDKEY), ESCCHAR, ESCCHAR, ULEN);
		else if(!strst(buf, FNKEY))
			esccpy(c->fn, robj(buf, UIDKEY), ESCCHAR, ESCCHAR, NALEN);
		else if(!strst(buf, ORGKEY))
			esccpy(c->org, robj(buf, UIDKEY), ESCCHAR, ESCCHAR, ORLEN);
		else if(!strst(buf, EMKEY))
			esccpy(c->em[c->emnum++], robj(buf, UIDKEY), ESCCHAR, ESCCHAR, EMLEN);
		else if(!strst(buf, PHKEY))
			esccpy(c->ph[c->phnum++], robj(buf, UIDKEY), ESCCHAR, ESCCHAR, PHLEN);
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
	else if(!strcmp("uid", cn)) strcpy(c->uid, ct);
	else if(!strcmp("fn", cn)) strcpy(c->fn, ct);
	else if(!strcmp("org", cn)) strcpy(c->org, ct);

	else if(!strcmp("ph", cn)) {
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

	else if(!strcmp("em", cn)) {
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
	esccpy(c->fn, buf, ESCCHAR, ESCCHAR, MBCH);

	if(randstr(buf, UCLEN)) strcpy(c->uid, buf);
	c->lid = getindex(db, verb);

	if(!readline("Organization", buf, ORLEN))
	esccpy(c->org, buf, ESCCHAR, ESCCHAR, MBCH);

	for(a = 0; a < PHNUM; a++) {
		snprintf(pstr, SBCH, "Phone %d", a);
		if(readline(pstr, buf, PHLEN)) break;
		esccpy(c->ph[a], buf, ESCCHAR, ESCCHAR, MBCH);
	}
	c->phnum = a;

	for(a = 0; a < EMNUM; a++) {
		snprintf(pstr, SBCH, "Email %d", a);
		if(readline(pstr, buf, EMLEN)) break;
		esccpy(c->em[a], buf, ESCCHAR, ESCCHAR, MBCH);
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
static int exec_import(sqlite3 *db, const flag *f, char **args, const char *cmd,
	const int numf) {

	int dbrc = 0;
	unsigned int a = 0;

	card **cc = dalloc(numf, sizeof(card));

	for(a = 0; a < numf; a++) {
		DIR *vd = opendir(args[a]);
		if(vd && !errno) {
			// TODO: Dirent walk

			closedir(vd);
		} else {
			FILE *vf = fopen(args[a], "r");
			if(vf == NULL) {
				errno = ENOENT;
				return usage(cmd);
			}

			icard(cc[a], vf, db, f->vfl);
			dbrc = wrcard(db, cc[a], f->op, f->vfl);
			if(dbrc) printf("SQL Error #: %d\n", dbrc);
			else if(f->vfl) printcard(cc[0], f->op, f->mxnum, f->vfl);
			fclose(vf);
		}
	}

	free(cc);
	return 0;
}

// Execute export operation
static int exec_export(sqlite3 *db, const flag *f, char **args,
	const int numarg) {

	unsigned int a = 0, cci = 0;

	char *sstr = calloc(BBCH, sizeof(char));
	char *fpath = calloc(MBCH, sizeof(char));

	card **cc = dalloc(f->mxnum, sizeof(card));
	strncpy(sstr, atostr(sstr, args, numarg), BBCH);

	setsrand();

	cci = mkdeck(db, cc, sstr, f->sfl, f->mxnum, f->vfl);
	for(a = 0; a < cci; a++) {
		ecard(cc[a], fpath, MBCH, f->vfl);
		if(f->vfl) printf("%s exported as: %s\n", cc[a]->fn, fpath);
	}

	free(sstr);
	free(fpath);
	free(cc);
	return 0;
}

// Execute 'new' operation TODO: Default values from args
static int exec_new(sqlite3 *db, const flag *f, const char *cmd) {

	int dbrc = 0;
	card *c = calloc(1, sizeof(card));

	setsrand();
	mknew(db, c, f->vfl);

	if(valcard(c)) {
		dbrc = wrcard(db, c, f->op, f->vfl);
		if(dbrc) printf("SQL Error #: %d\n", dbrc);
		else if(f->vfl) printcard(c, f->op, f->mxnum, f->vfl);

	} else {
		errno = EINVAL;
		return usage(cmd);
	}

	free(c);
	return 0;
}

// Execute delete operation
static int exec_delete(sqlite3 *db, const flag *f, char **args,
	const int numarg) {

	card **cc = dalloc(1, sizeof(card));

	mkdeck(db, cc, args[0], lid, f->mxnum, f->vfl);
	if(valcard(cc[0])) {
		delcard(db, cc[0]->lid, f->vfl);
		if(f->vfl) {
			printf("Deleted card:\n");
			printcard(cc[0], f->op, f->mxnum, 2);
		}
	} else {
		printf("Found no contact with ID #%d.\n", matoi(args[0]));
	}

	free(cc);
	return 0;
}

// Execute search operations (all, phone & mail)
static int exec_search(sqlite3 *db, const flag *f, char **args, const int numarg) {

	unsigned int a = 0, cci = 0;

	char *sstr = calloc(BBCH, sizeof(char));
	card **cc = dalloc(f->mxnum, sizeof(card));

	strncpy(sstr, atostr(sstr, args, numarg), BBCH);

	cci = mkdeck(db, cc, sstr, f->sfl, f->mxnum, f->vfl);
	for(a = 0; a < cci; a++) printcard(cc[a], f->op, f->mxnum, f->vfl);

	free(sstr);
	free(cc);
	return 0;
}

// Execute operations
static int execute(sqlite3 *db, const flag *f,  const char *cmd,
	const int alen, char **args) {
	errno = 0;

	switch(f->op) {

		case help:
			return usage(cmd);
		case create:
			return exec_create(db, cmd);
		case import:
			return exec_import(db, f, args, cmd, alen);
		case export:
			return exec_export(db, f, args, alen);
		case new:
			return exec_new(db, f, cmd);
		case delete:
			return exec_delete(db, f, args, alen);
		default:
			return exec_search(db, f, args, alen);
	}
}

// Set all flags to default values
void iflag(flag *f) {

	f->op = 0;
	f->vfl = 0;
	f->mxnum = NUMCARD;
	f->sfl = -1;
}

int main(int argc, char **argv) {

	char *cmd = calloc(MBCH, sizeof(char));

	flag *f = calloc(1, sizeof(flag));
	iflag(f);

	sqlite3 *db;

	int ret = 0;
	int optc;

	strncpy(cmd, basename(argv[0]), MBCH);

	while((optc = getopt(argc, argv, "hn:v")) != -1) {
		switch(optc) {

			case 'h':
				return usage(cmd);
				break;

			case 'n':
				f->mxnum = matoi(optarg);
				if(f->mxnum > NUMCARD) f->mxnum = NUMCARD;
				break;

			case 'v':
				f->vfl++;
				break;

			default:
				return usage(cmd);
				break;
		}
	}

	
	// Open database
	if(sqlite3_open(DBNAME, &db)) errno = ENOENT;
	if(errno) return usage(cmd);

	// Dump DB if no argument has been given
	if(argc < optind + 1) {
		f->op = all;	
		ret = exec_search(db, f, NULL, 0);
		sqlite3_close(db);
		return ret;
	}

	// Set and verify operation
	f->op = chops(argv[optind]);
	if(f->op < 0) errno = EINVAL;
	if(errno) return usage(cmd);

	// Open database
	int dbrc = sqlite3_open(DBNAME, &db);
	if(dbrc) errno = ENOENT;
	if(errno) return usage(cmd);

	// Prepare argument list
	argc -= optind + 1;
	argv += optind + 1;

	// Initiate operations
	ret = execute(db, f, cmd, argc, argv);

	// Graceful exit
	free(cmd);
	free(f);
	sqlite3_close(db);
	return ret;
}
