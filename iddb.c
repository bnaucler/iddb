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

int usage(const char *err, const char *cmd) {

	if(err[0]) fprintf(stderr, "Error: %s\n", err);
	if(errno) fprintf(stderr, "Error: %s\n", strerror(errno));

	printf("%s %s\n", cmd, VER);
	printf("Usage: %s [options] [operation] [args]\n\n", cmd);
	printf("Operations:\n"
			"    h[elp]    Display this text\n"
			"    c[reate]  Create or reset database\n"
			"    a[ll]     Search and display all contact information\n"
			"    i[mport]  Import VCF from file(s) or dir(s)\n"
			"    e[xport]  Search and export output to VCF\n"
			"    p[hone]   Search and display phone numbers\n"
			"    r[aw]     Interactively add user from raw email dump\n"
			"    m[ail]    Search and display email addresses\n"
			"    n[ew]     Interactively add new contact\n"
			"    d[elete]  Delete user with specified ID\n\n"

			"Options:\n"
			"    -d dir    Specify output directory for export\n"
			"    -f file   Specify database file\n"
			"    -h        Display this text\n"
			"    -n num    Display max num results\n"
			"    -v        Increase verbosity level:\n");

	return errno;
}

// Check for valid operation
static int chops(const char *cop, size_t mxl) {

	char vops[10][7] = {"create", "delete", "import", "export", "help",
		"phone", "raw", "mail", "new", "all"};

	int clen = strlen(cop);
	if(clen > mxl) return -1;

	char lccop[(clen + 1)];
	int opnum = sizeof(vops) / sizeof(vops[0]);

	unsigned int a = 0;

	for(a = 0; a < clen; a++) lccop[a] = tolower(cop[a]);
	lccop[clen] = 0;

	for(a = 0; a < opnum; a++) {
		if(clen == 1) { if(lccop[0] == vops[a][0]) return a; }
		else { if(!strcmp(lccop, vops[a])) return a; }
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

	snprintf(sql, BBCH, "SELECT COUNT(*) FROM id;");

	int dbop = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	if (verb > 1) printf("Query: %s\n", sql);

	while((dbop = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(dbop == SQLITE_ROW) ret = matoi((char*)sqlite3_column_text(stmt, a));
	}

	free(sql);
	return ret;
}

// Export card to file
static char *ecard(card *c, char *fpath, int psz, const char *edir,
	const int verb) {

	char *rstr = calloc(EXPLEN + 1, sizeof(char));
	unsigned int a = 0;

	randstr(rstr, EXPLEN);

	if(!edir[0]) strncpy(fpath, getenv("PWD"), MBCH);
	else strncpy(fpath, edir, MBCH);

	int dlen = strlen(fpath);
	if(fpath[(dlen)] != DDIV) fpath[(dlen)] = DDIV;
	fpath[(dlen + 1)] = '\0';

	strncat(fpath, rstr, MBCH);
	strncat(fpath, EXPSUF, MBCH);

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

// Import card to struct TODO: Support for files with multiple cards
static card *icard(card *c, FILE *f, sqlite3 *db, const int verb) {

	char *buf = malloc(MBCH);
	char *sbuf = malloc(SBCH);
	char *fphone = malloc(PHLEN);
	int verc = 0;

	c->phnum = 0;
	c->emnum = 0;

	while(fgets(buf, MBCH, f)){

		if(!strst(buf, STARTKEY) || !strst(buf, STOPKEY)) {
			verc++;

		} else if(!strst(buf, UIDKEY)) {
			esccpy(c->uid, robj(buf), ESCCHAR, ESCCHAR, ULEN);

		} else if(!strst(buf, FNKEY)) {
			esccpy(c->fn, robj(buf), ESCCHAR, ESCCHAR, NALEN);

		} else if(!strst(buf, ORGKEY)) {
			esccpy(c->org, robj(buf), ESCCHAR, ESCCHAR, ORLEN);

		} else if(!strst(buf, EMKEY)) {
			if(isemail(buf))
				esccpy(c->em[c->emnum++], robj(buf), ESCCHAR, ESCCHAR, EMLEN);

		} else if(!strst(buf, PHKEY)) {
			strncpy(sbuf, robj(buf), SBCH);
			if(isphone(sbuf, PHLEN)) {
				esccpy(c->ph[c->phnum++], formphone(fphone, sbuf),
					ESCCHAR, ESCCHAR, PHLEN);
			}
		}
	}

	if(verc == 2) c->lid = getindex(db, verb) + 1;

	free(buf);
	free(sbuf);
	free(fphone);
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

	free(sql);
	return dbrc;
}

// Write struct to DB
int wrcard(sqlite3 *db, card *c, const int op, const int verb) {

	char *sql = calloc(BBCH, sizeof(char));
	char *pbuf = calloc(PHNUM * PHLEN, sizeof(char));
	char *mbuf = calloc(EMNUM * EMLEN, sizeof(char));

	char *err = 0;

	snprintf(sql, BBCH, "INSERT INTO id VALUES"
			"(%d, '%s', '%s', '%s',  '%s', '%s');",
			c->lid, c->uid, c->fn, c->org,
			marshal(pbuf, c->phnum, PHLEN, c->ph),
			marshal(mbuf, c->emnum, EMLEN, c->em));

	if(verb > 1) printf("Query: %s\n", sql);

	int dbrc = sqlite3_exec(db, sql, 0, 0, &err);

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
	else if(!strcmp("uid", cn)) strncpy(c->uid, ct, ULEN);
	else if(!strcmp("fn", cn)) strncpy(c->fn, ct, NALEN);
	else if(!strcmp("org", cn)) strncpy(c->org, ct, ORLEN);

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

// Create SQL search string from svar
static char *mksqlstr(int svar, char *sql, const char *str) {

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

	return sql;
}

// Return entry from database
static int searchdb(sqlite3 *db, card *c, const flag *f, char *sql,
	unsigned int cci) {

	sqlite3_stmt *stmt;
	card *head = c;
	card *cmpc = c;

	unsigned int a = 0, isdbl = 0;

	if (f->vfl > 1) printf("Query: %s\n", sql);

	int dbrc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

	if (dbrc && f->vfl) fprintf(stderr, "SQL error: %d\n", dbrc);

	while((dbrc = sqlite3_step(stmt)) != SQLITE_DONE) {
		if(dbrc == SQLITE_ROW) {
			int ccnt = sqlite3_column_count(stmt);
			for(a = 0; a < ccnt; a++){
				readid(c, sqlite3_column_name(stmt, a),
						(char*)sqlite3_column_text(stmt, a));
			}
			cci++;
		}

		if(!c->next) c->next = calloc(1, sizeof(card));
		cmpc = head;

		while(cmpc->lid) {
			if(!cmpcard(cmpc, c)) isdbl++;
			cmpc = cmpc->next;
		}

		if(isdbl < 2 || c == head) {
			c = c->next;
			if(cci >= f->mxnum) return cci;
		}

		isdbl = 0;
	}

	return cci;
}

// Create deck of cards - init search(es)
static int mkdeck(sqlite3 *db, card *c, const flag *f, const char *str) {

	char *sql = calloc(BBCH, sizeof(char));
	unsigned int a = 0, cci = 0;

	if(!str) {
		cci = searchdb(db, c, f, "SELECT * FROM id;", cci);

	} else if (f->sfl < 0) {
		for(a = 0; a < 6; a++) {
			if(cci >= f->mxnum) return cci;
			cci = searchdb(db, c, f, mksqlstr(a, sql, str), cci);
		}

	} else {
		cci = searchdb(db, c, f, mksqlstr(f->sfl, sql, str), cci);
	}

	free(sql);
	return cci;
}

// Read new card from stdin
static int mknew(sqlite3 *db, card *c, const int verb) {

	char *buf = calloc(MBCH, sizeof(char));
	char *fphone = calloc(PHLEN, sizeof(char));
	char *prompt = calloc(MBCH, sizeof(char));

	unsigned int a = 0;
	int rlrc = 0;

	if((rlrc = readline("Full name", buf, c->fn, NALEN)) && !c->fn[0]) return 1;
	else if(!rlrc) esccpy(c->fn, buf, ESCCHAR, ESCCHAR, MBCH);

	if(randstr(buf, UCLEN)) strcpy(c->uid, buf);

	c->lid = getindex(db, verb) + 1;

	if(!readline("Organization", buf, c->org, ORLEN))
		esccpy(c->org, buf, ESCCHAR, ESCCHAR, MBCH);

	for(a = 0; a < PHNUM; a++) {
		snprintf(prompt, MBCH, "Phone %d", a);
		if(readline(prompt, buf, "", PHLEN)) break;
		if(isphone(buf, PHLEN)) {
			esccpy(c->ph[a], formphone(fphone, buf), ESCCHAR, ESCCHAR, MBCH);
		} else return 1;
	}
	c->phnum = a;

	for(a = 0; a < EMNUM; a++) {
		snprintf(prompt, MBCH, "Email %d", a);
		if((rlrc = readline(prompt, buf, c->em[a], EMLEN)) && !c->em[a][0])
			break;
		else if(!rlrc) esccpy(c->em[a], buf, ESCCHAR, ESCCHAR, MBCH);
	}
	c->emnum = a;

	free(buf);
	free(fphone);
	free(prompt);

	return 0;
}

// Deletes card #lid from the database
static int delcard(sqlite3 *db, int clid, int verb) {

	int last = getindex(db, verb);
	char *sql = calloc(BBCH, sizeof(char));
	char *err = 0;

	snprintf(sql, BBCH, "DELETE FROM id WHERE lid=%d;", clid);
	if(last != clid) { if(mvcard(db, clid, last)) return -1; }

	int dbrc = sqlite3_exec(db, sql, 0, 0, &err);

	free(sql);
	return dbrc;
}

// Wrapper for icard()
static int iloop(sqlite3 *db, const flag *f, const char *fname) {

	FILE *vf = fopen(fname, "r");
	card *c = calloc(1, sizeof(card));

	icard(c, vf, db, f->vfl);
	int dbrc = wrcard(db, c, f->op, f->vfl);

	if(dbrc) return dbrc;
	else if(f->vfl) printcard(c, f);

	fclose(vf);
	free(c);
	return 0;
}

// Execute import loop for each file in directory
static int import_dir(sqlite3 *db, DIR *vd, const flag *f, char *fpath,
	char *dirn, unsigned int ctr) {

	struct dirent *dir;

	while((dir = readdir(vd)) != NULL) {
		if(dir->d_name[0] != '.') {
			if(!mkpath(fpath, dirn, dir->d_name, DDIV, MBCH)) {
				if(iloop(db, f, fpath)) {
					fprintf(stderr, "Error reading file %s\n", fpath);
				} else {
					ctr++;
					if(f->vfl) printf("[%d] %s imported successfully\n", ctr, fpath);
				}
			}
		}
	}

	return ctr;
}

// Fill default values to card from args
static card *setcarddef(card *c, char **args, const int anum) {

	unsigned int a = 0, hasem = 0;

	for(a = 0; a < anum; a++) {
		if(isemail(args[a]) && hasem < EMNUM) {
			strncpy(c->em[hasem++], args[a], EMLEN);

		} else if(!hasem) {
			if(c->fn[0]) strncat(c->fn, " ", NALEN);
			strncat(c->fn, args[a], NALEN);

		} else {
			if(c->org[0]) strncat(c->org, " ", ORLEN);
			strncat(c->org, args[a], ORLEN);
		}
	}

	return c;
}

// Execute create operation
static int exec_create(sqlite3 *db, const char *cmd, const char *dbpath) {

	if(ctable(db)) return usage("Could not create/reset database", cmd);
	else printf("Database %s created successfully\n", dbpath);

	return 0;
}

// Execute import operation
static int exec_import(sqlite3 *db, const flag *f, char **args, const char *cmd,
	const int numf) {

	unsigned int a = 0, ctr = 0;
	DIR *vd;

	char *fpath = calloc(MBCH, sizeof(char));

	for(a = 0; a < numf; a++) {
		errno = 0;
		vd = opendir(args[a]);

		if(vd && !errno) {
			ctr += import_dir(db, vd, f, fpath, args[a], ctr);
			closedir(vd);

		} else {
			if(iloop(db, f, args[a])) {
				fprintf(stderr, "Error reading file %s\n", args[a]);
			} else {
				ctr++;
				if(f->vfl) printf("[%d] %s imported successfully\n", ctr, args[a]);
			}
		}
	}

	if(f->vfl) printf("%d File(s) imported\n", ctr);

	free(fpath);
	return 0;
}

// Execute export operation
static int exec_export(sqlite3 *db, const flag *f, char **args,
	const int numarg) {

	char *sstr = calloc(BBCH, sizeof(char));
	char *fpath = calloc(MBCH, sizeof(char));
	card *head = calloc(1, sizeof(card));
	card *ccard = head;

	strncpy(sstr, atostr(sstr, args, numarg), BBCH);
	mkdeck(db, ccard, f, sstr);

	ccard = head;

	while(ccard->lid) {
		ecard(ccard, fpath, MBCH, f->dfl, f->vfl);
		if(f->vfl) printf("%s exported as: %s\n", ccard->fn, fpath);
		ccard = ccard->next;
	}

	free(sstr);
	free(fpath);
	return 0;
}

// Execute 'new' operation
static int exec_new(sqlite3 *db, const flag *f, const char *cmd,
	char **args, const int anum) {

	int dbrc = 0;

	card *c = calloc(1, sizeof(card));
	c = setcarddef(c, args, anum);

	if(mknew(db, c, f->vfl)) return 1;

	if(valcard(c)) {
		if((dbrc = wrcard(db, c, f->op, f->vfl))) printf("SQL Error #: %d\n", dbrc);
		else if(f->vfl) printcard(c, f);

	} else {
		return usage("Invalid contact format", cmd);
	}

	free(c);
	return 0;
}

// Execute delete operation
static int exec_delete(sqlite3 *db, const flag *f, char **args,
	const int numarg) {

	card *head = calloc(1, sizeof(card));
	card *ccard = head;

	mkdeck(db, ccard, f, args[0]);

	if(valcard(ccard)) {
		delcard(db, head->lid, f->vfl);
		if(f->vfl) {
			printf("Deleted card:\n");
			printcard(ccard, f);
		}
	} else {
		printf("Found no contact with ID #%d.\n", matoi(args[0]));
	}

	return 0;
}

// Execute search operations (all, phone & mail)
static int exec_search(sqlite3 *db, const flag *f, char **args, const int numarg) {

	char *sstr = calloc(BBCH, sizeof(char));
	char *tstr = calloc(BBCH, sizeof(char));

	card *head = calloc(1, sizeof(card));
	card *ccard = head;

	strncpy(sstr, atostr(tstr, args, numarg), BBCH);

	mkdeck(db, ccard, f, sstr);
	ccard = head;

	while(ccard->lid) {
		printcard(ccard, f);
		ccard = ccard->next;
	}

	free(sstr);
	free(tstr);
	return 0;
}

// Execute 'raw' operation TODO: Get more data from signature
static int exec_raw(sqlite3 *db, const flag *f) {

	char *buf = calloc(BBCH, sizeof(char));
	char *nbuf = calloc(BBCH, sizeof(char));

	card *c = calloc(1, sizeof(card));

	int ret = 0;
	unsigned int a = 0, b = 0, d = 0, isem = 0;

	// TODO: Make less ugly
	while(fgets(buf, BBCH, stdin)) {
		if(!strst(buf, FRKEY)) {
			strncpy(nbuf, robj(buf), BBCH);
			int slen = strlen(nbuf);
			for(a = 0; a < slen; a++) {
				if(nbuf[a] == '<') isem++;
				else if(nbuf[a] == '>') break;
				else if(!isem) {
					if(!c->fn[0] && isspace(nbuf[a])) {} // nothing
					else c->fn[b++] = nbuf[a];
				}
				else c->em[0][d++] = nbuf[a];
			}

		}
	}

	if(c->fn[0]) remtchar(c->fn, ' ');

	ret = mknew(db, c, f->vfl);

	if(!ret && valcard(c)) {
		if((ret = wrcard(db, c, f->op, f->vfl))) printf("SQL Error #: %d\n", ret);
		else if(f->vfl) printcard(c, f);
	} 

	free(buf);
	return ret;
}

// Execute operations
static int execute(sqlite3 *db, const flag *f,  const char *cmd,
	const char *dbpath, const int alen, char **args) {

	errno = 0;

	switch(f->op) {

		case help:
			return usage("", cmd);
		case create:
			return exec_create(db, cmd, dbpath);
		case import:
			return exec_import(db, f, args, cmd, alen);
		case export:
			return exec_export(db, f, args, alen);
		case new:
			return exec_new(db, f, cmd, args, alen);
		case delete:
			return exec_delete(db, f, args, alen);
		case raw:
			return exec_raw(db, f);
		default:
			return exec_search(db, f, args, alen);
	}
}

// Set all flags to default values
static void iflag(flag *f) {

	f->op = -1;
	f->mxnum = NUMCARD;
	f->vfl = 0;
	f->sfl = -1;
}

// TODO: operation for adding email from raw dump
int main(int argc, char **argv) {

	char *cmd = calloc(MBCH, sizeof(char));
	char *dbpath = calloc(MBCH, sizeof(char));

	flag *f = calloc(1, sizeof(flag));
	iflag(f);

	sqlite3 *db;

	int ret = 0;
	int optc;

	setsrand();

	strncpy(cmd, basename(argv[0]), MBCH);

	while((optc = getopt(argc, argv, "d:f:hn:v")) != -1) {
		switch(optc) {

			case 'd':
				strncpy(f->dfl, optarg, MBCH);
				break;

			case 'f':
				strncpy(dbpath, optarg, MBCH);
				break;

			case 'h':
				return usage("", cmd);
				break;

			case 'n':
				f->mxnum = matoi(optarg);
				if(f->mxnum > NUMCARD) f->mxnum = NUMCARD;
				else if(f->mxnum < 1)
					return usage("n cannot be zero or negative", cmd);
				break;

			case 'v':
				f->vfl++;
				break;

			default:
				return usage("", cmd);
				break;
		}
	}

	// Open database
	if(!dbpath[0])
		snprintf(dbpath, MBCH, "%s%c%s", getenv("HOME"), DDIV, DBNAME);
	if(sqlite3_open(dbpath, &db)) errno = ENOENT;
	else errno = 0;

	// Dump DB if no argument has been given
	if(argc <= optind && !errno) {
		f->op = all;
		ret = exec_search(db, f, NULL, 0);

		sqlite3_close(db);
		return ret;

	// Create database if it doesn't exist
	} else if(errno) {
		exec_create(db, cmd, dbpath);
		errno = 0;
		if(argc <= optind) return 0;
	}

	// Set and verify operation
	f->op = chops(argv[optind], SBCH);
	if(f->op < 0) errno = EINVAL;
	if(errno) return usage("", cmd);

	// Prepare argument list
	argc -= optind + 1;
	argv += optind + 1;

	// Initiate operations
	ret = execute(db, f, cmd, dbpath, argc, argv);

	// Graceful exit
	free(cmd);
	free(dbpath);
	free(f);
	sqlite3_close(db);
	return ret;
}
