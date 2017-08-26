/*
 *
 *        cfunc.c - card handling functions for iddb
 *
 */

#include "iddb.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

// Return 0 if card has LID and FN, else return 1
int valcard(card *c) {

    if(c->lid > 0 && c->fn[0]) return 0;
    else return 1;
}

// Change card lid from plid to nlid
int mvcard(sqlite3 *db, int plid, int nlid) {

    char *sql = calloc(BBCH, sizeof(char));
    char *err = 0;

    snprintf(sql, BBCH, "UPDATE id SET lid=%d WHERE lid=%d;", nlid, plid);
    int dbrc = sqlite3_exec(db, sql, NULL, NULL, &err);

    free(sql);
    return dbrc;
}

// Return 0 if c1 and c2 are identical
int cmpcard(const card *c1, const card *c2) {

    if(c1->lid == c2->lid) return 1;
    if(strncmp(c1->uid, c2->uid, ULEN)) return 2;
    if(strncmp(c1->fn, c2->fn, NALEN)) return 3;
    if(strncmp(c1->org, c2->org, ORLEN)) return 4;
    if(c1->phnum != c2->phnum) return 5;
    if(c1->emnum != c2->emnum) return 6;

    return 0;
}

// Print card to stdout TODO: Avoid duplicates at op phone and mail
int printcard(card *c, const flag *f) {

    unsigned int a = 0;

    if(!c->lid) return 1;

    if(f->op == all || f->op == new || f->op == delete || f->op == raw) {
        if(f->vfl) printf("%d: ", c->lid);
        printf("%s ", c->fn);
        if(c->org[0]) printf("(%s)\n", c->org);
        else printf("\n");
        if(c->uid[0] && f->vfl) printf("UID: %s\n", c->uid);

        for(a = 0; a < c->emnum && a < f->mxnum; a++)
            printf("email %d: %s\n", a, c->em[a]);
        for(a = 0; a < c->phnum && a < f->mxnum; a++)
            printf("phone %d: %s\n", a, c->ph[a]);
        printf("\n");

    } else if(f->op == mail) {
        if(f->vfl) printf("%d: %s\n", c->lid, c->fn);
        for(a = 0; a < c->emnum && a < f->mxnum; a++) {
            if(f->vfl) printf("email %d: ", a);
            printf("%s\n", c->em[a]);
        }

    } else if(f->op == phone) {
        if(f->vfl) printf("%d: %s\n", c->lid, c->fn);
        for(a = 0; a < c->phnum && a < f->mxnum; a++) {
            if(f->vfl) printf("phone %d: ", a);
            printf("%s\n", c->ph[a]);
        }
    }

    return 0;
}

// Write struct to DB
int wrcard(sqlite3 *db, card *c, const int op, const int verb) {

    char *sql = calloc(BBCH, sizeof(char));
    char *pbuf = calloc(PHNUM * PHLEN, sizeof(char));
    char *mbuf = calloc(EMNUM * EMLEN, sizeof(char));

    char *err = 0;
    int dbrc = 0;

    while(c->lid) {
        snprintf(sql, BBCH, "INSERT INTO id VALUES"
                "(%d, '%s', '%s', '%s',  '%s', '%s');",
                c->lid, c->uid, c->fn, c->org,
                marshal(pbuf, c->phnum, PHLEN, c->ph),
                marshal(mbuf, c->emnum, EMLEN, c->em));

        if(verb > 1) printf("Query: %s\n", sql);

        dbrc = sqlite3_exec(db, sql, 0, 0, &err);

        if(c->next) c = c->next;
        else break;
    }

    free(sql);
    free(pbuf);
    free(mbuf);

    return dbrc;
}
