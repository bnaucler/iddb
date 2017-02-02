/*
 *
 *		cfunc.c - card handling functions for iddb
 *
 */

#include "iddb.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

// Return 1 if card has LID and FN, else return 0
int valcard(card *c) {

	if(c->lid > 0 && c->fn[0]) return 1;
	else return 0;
}

// Change card LID from plid to nlid
int mvcard(sqlite3 *db, int plid, int nlid) {

	char *sql = calloc(BBCH, sizeof(char));
	char *err = 0;

	snprintf(sql, BBCH, "UPDATE id SET lid=%d WHERE lid=%d;", nlid, plid);

	int dbrc = sqlite3_exec(db, sql, 0, 0, &err);
	free(sql);
	return dbrc;
}

// Return 0 if c1 and c2 are identical
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

// Print card to stdout
int printcard(card *c, const int op, const int mxnum, const int verb) {

	unsigned int a = 0;

	if(!c->lid) return 1;

	if(op == all || op == new || op == delete) {
		if(verb) printf("%d: ", c->lid);
		printf("%s ", c->fn);
		if(c->org[0]) printf("(%s)\n", c->org);
		else printf("\n");
		if(c->uid[0] && verb) printf("UID: %s\n", c->uid);

		for(a = 0; a < c->emnum && a < mxnum; a++)
			printf("email %d: %s\n", a, c->em[a]);
		for(a = 0; a < c->phnum && a < mxnum; a++)
			printf("phone %d: %s\n", a, c->ph[a]);
		printf("\n");

	} else if(op == mail) {
		if(verb) printf("%d: %s\n", c->lid, c->fn);
		for(a = 0; a < c->emnum && a < mxnum; a++) {
			if(verb) printf("email %d: ", a);
			printf("%s\n", c->em[a]);
		}
	} else if(op == phone) {
		if(verb) printf("%d: %s\n", c->lid, c->fn);
		for(a = 0; a < c->phnum && a < mxnum; a++) {
			if(verb) printf("phone %d: ", a);
			printf("%s\n", c->ph[a]);
		}
	}

	return 0;
}
