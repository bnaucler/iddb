/*
 *
 *        iddb.c - Identity database / VCF parser
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

int usage(const char *err, const flag *f) {

    if(err[0]) fprintf(stderr, "Error: %s\n", err);
    if(errno) fprintf(stderr, "Error: %s\n", strerror(errno));

    printf("%s %s\n", f->cmd, VER);
    printf("Usage: %s [options] [operation] [args]\n\n", f->cmd);
    printf("Operations:\n"
           "    h[elp]    Display this text\n"
           "    c[reate]  Create or reset database\n"
           "    a[ll]     Search and display all contact information\n"
           "    i[mport]  Import VCF from file(s) or dir(s)\n"
           "    j[oin]    Interactively merge two cards based on ID\n"
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
// TODO: Make non-hacky
static int chops(const char *cop, size_t mxl) {

    char vops[11][7] = {
        "create", "delete", "import",
        "join", "export", "help",
        "phone", "raw", "mail",
        "new", "all"
    };

    int clen = strlen(cop);
    if(clen > mxl) return -1;

    char lccop[(clen + 1)];
    int opnum = sizeof(vops) / sizeof(vops[0]);

    unsigned int a = 0;

    for(a = 0; a < clen; a++) lccop[a] = tolower(cop[a]);
    lccop[clen] = 0;

    for(a = 0; a < opnum; a++) {
        if(clen == 1) { if(lccop[0] == vops[a][0]) return a; }
        else { if(!strncmp(lccop, vops[a], 7)) return a; }
    }

    errno = EINVAL;
    return -1;
}

// Create file name based on c->fn
static int mkfname(const card *c, char *str, const int i, size_t mxl) {

    int a = 0;
    char isuf[5];

    if(str[0]) memset(str, 0, mxl);

    if(i > 99) exit(2); // TODO: Graceful exit for edge cases
    else if(i) mxl -= 5;

    while(c->fn[a] && a < mxl) {
        if(isspace(c->fn[a])) str[a] = '_';
        else str[a] = tolower(c->fn[a]);
        a++;
    }

    if(i) {
        snprintf(isuf, 5, "_%02d", i);
        strncat(str, isuf, mxl + 4);

    } else if(a < 2) {
        randstr(str, 20);

    } else {
        str[++a] = 0;
    }

    return a;
}

// Concat full file path
static int catfpath(char *path, const char *fn, const char *suf,
    const int mxsz, const flag *f) {

    if(!f->dfl[0]) strncpy(path, getenv("PWD"), MBCH);
    else strncpy(path, f->dfl, MBCH);

    int dlen = strlen(path);

    if(path[(dlen)] != DDIV) path[(dlen)] = DDIV;
    path[(dlen + 1)] = 0;

    strncat(path, fn, mxsz);
    strncat(path, suf, mxsz);

    return access(path, W_OK);
}

// Export card to file
static char *ecard(card *c, const flag *f, char *fpath) {

    char *fn = calloc(EXPLEN, sizeof(char));

    unsigned int a = 0, i = 0;

    do mkfname(c, fn, i++, EXPLEN);
    while(!catfpath(fpath, fn, EXPSUF, MBCH, f));

    FILE *cf = fopen(fpath, "w");

    fprintf(cf, "BEGIN:VCARD\nVERSION:3.0\n");
    fprintf(cf, "FN:%s\n", c->fn);
    if(c->uid[0]) fprintf(cf, "UID:%s\n", c->uid);
    if(c->org[0]) fprintf(cf, "ORG:%s\n", c->org);
    for(a = 0; a < c->emnum; a++) fprintf(cf, "EMAIL;TYPE=INTERNET:%s\n", c->em[a]);
    for(a = 0; a < c->phnum; a++) fprintf(cf, "TEL;TYPE=VOICE:%s\n", c->ph[a]);
    fprintf(cf, "END:VCARD\n");

    fclose(cf);
    free(fn);

    return fpath;
}

// Read card object from string to struct
static int readcardobj(card *c, char *buf, char *sbuf) {

    if(!strst(buf, STARTKEY) || !strst(buf, STOPKEY)) {
        return 1;

    } else if(!strst(buf, UIDKEY)) {
        esccpy(c->uid, robj(buf, ULEN), ESCCHAR, ESCCHAR, ULEN);

    } else if(!strst(buf, FNKEY)) {
        esccpy(c->fn, robj(buf, NALEN), ESCCHAR, ESCCHAR, NALEN);

    } else if(!strst(buf, ORGKEY)) {
        esccpy(c->org, robj(buf, ORLEN), ESCCHAR, ESCCHAR, ORLEN);

    } else if(!strst(buf, EMKEY)) {
        if(isemail(buf))
            esccpy(c->em[c->emnum++], robj(buf, EMLEN), ESCCHAR, ESCCHAR, EMLEN);

    } else if(!strst(buf, PHKEY)) {
        strncpy(sbuf, robj(buf, PHLEN), SBCH);
        if(isphone(sbuf, PHLEN)) {
            esccpy(c->ph[c->phnum++], formphone(buf, sbuf),
                ESCCHAR, ESCCHAR, PHLEN);
        }
    }

    return 0;
}

// Import card to struct
static card *icard(card *head, FILE *f, sqlite3 *db, const int verb) {

    char *buf = malloc(MBCH);
    char *sbuf = malloc(SBCH);
    int verc = 0, cnum = 1;

    card *c = head;

    while(fgets(buf, MBCH, f)){
        verc += readcardobj(c, buf, sbuf);

        if(verc == 2) {
            c->lid = getindex(db, verb) + cnum++;
            c->next = calloc(1, sizeof(card));
            c = c->next;
            verc = 0;
        }
    }

    free(buf);
    free(sbuf);

    return head;
}

// Create (or reset) database table
static int ctable(sqlite3 *db) {

    char *sql = calloc(DBCH, sizeof(char));
    char *err = 0;

    strncpy(sql, "DROP TABLE IF EXISTS id;"
                 "CREATE TABLE id(lid INT, uid TEXT, fn TEXT,"
                 " org TEXT, ph TEXT, em TEXT);", DBCH);

    int dbrc = sqlite3_exec(db, sql, 0, 0, &err);

    free(sql);
    return dbrc;
}

// Unmarshal array from SQL query
static int sqlunmarshal(const char *ct, char **tarr, unsigned int *slen) {

    unsigned int a = 0;

    *slen = ct[1];
    for(a = 0; a < ct[0]; a++) tarr[a] = calloc(*slen + 1, sizeof(char));
    unmarshal(ct, tarr);

    return (int) ct[0];
}

// Read SQL data into card
static card *readid(card *c, const char *cn, const char *ct) {

    char **tarr = calloc(EMNUM * EMLEN, sizeof(char));
    unsigned int a = 0, slen = 0;

    if(!strcmp("lid", cn)) c->lid = matoi(ct);
    else if(!strcmp("uid", cn)) strncpy(c->uid, ct, ULEN);
    else if(!strcmp("fn", cn)) strncpy(c->fn, ct, NALEN);
    else if(!strcmp("org", cn)) strncpy(c->org, ct, ORLEN);

    else if(!strcmp("ph", cn)) {
        c->phnum = sqlunmarshal(ct, tarr, &slen);
        for(a = 0; a < c->phnum; a++) strncpy(c->ph[a], tarr[a], slen);

    } else if(!strcmp("em", cn)) {
        c->emnum = sqlunmarshal(ct, tarr, &slen);
        for(a = 0; a < c->emnum; a++) strncpy(c->em[a], tarr[a], slen);
    }

    free(tarr);
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

// Check linked list for doubles and advance if none is found
static card *chkdblcard(card *c, card *head) {

    unsigned int isdbl = 0;
    card *cmpc = head;

    while(cmpc->lid) {
        if(!cmpcard(cmpc, c)) isdbl++;
        cmpc = cmpc->next;
    }

    if(isdbl < 2 || c == head) c = c->next;

    return c;
}

// Return entry from database
static int searchdb(sqlite3 *db, card *c, const flag *f, char *sql,
    unsigned int cci) {

    sqlite3_stmt *stmt;
    card *head = c;

    unsigned int a = 0;

    if (f->vfl > 1) printf("Query: %s\n", sql);

    int dbrc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    if (dbrc && f->vfl) fprintf(stderr, "SQL error: %d\n", dbrc);

    while((dbrc = sqlite3_step(stmt)) != SQLITE_DONE) {
        if(dbrc == SQLITE_ROW) {
            if(!c->next) c->next = calloc(1, sizeof(card));
            int ccnt = sqlite3_column_count(stmt);
            for(a = 0; a < ccnt; a++){
                readid(c, sqlite3_column_name(stmt, a),
                        (char*)sqlite3_column_text(stmt, a));
            }
            cci++;
        }

        c = chkdblcard(c, head);
        if(cci >= f->mxnum) return cci;
    }

    return cci;
}

// Create deck of cards - init search(es)
static int mkdeck(sqlite3 *db, card *c, const flag *f, const char *str) {

    char *sql = calloc(BBCH, sizeof(char));
    unsigned int a = 0, cci = 0;

    if(!str) {
        cci = searchdb(db, c, f, "SELECT * FROM id;", cci);

    // TODO: repair handling of duplicates over multiple searches
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

// Interactively add new phone number
static int mknewphone(card *c, char *prompt, char *buf) {

    char *fphone = calloc(PHLEN, sizeof(char));

    unsigned int a = 0;

    for(a = 0; a < PHNUM; a++) {
        snprintf(prompt, MBCH, "Phone %d", a);
        if(readline(prompt, buf, "", PHLEN)) break;
        if(isphone(buf, PHLEN))
            esccpy(c->ph[a], formphone(fphone, buf), ESCCHAR, ESCCHAR, MBCH);
        else return 1;
    }

    c->phnum = a;

    free(fphone);
    return 0;
}

// Interactively add new email
static int mknewemail(card *c, char *prompt, char *buf) {

    unsigned int a = 0, rlrc = 0;

    for(a = 0; a < EMNUM; a++) {
        snprintf(prompt, MBCH, "Email %d", a);
        if((rlrc = readline(prompt, buf, c->em[a], EMLEN)) && !c->em[a][0]) break;
        else if(!rlrc) {
            if(isemail(buf)) esccpy(c->em[a], buf, ESCCHAR, ESCCHAR, MBCH);
            else return 1;
        }
    }

    c->emnum = a;

    return 0;
}

// Interactively add new card
static int mknew(sqlite3 *db, card *c, const int verb) {

    char *buf = calloc(MBCH, sizeof(char));
    char *prompt = calloc(MBCH, sizeof(char));

    unsigned int rlrc = 0;

    if((rlrc = readline("Full name", buf, c->fn, NALEN)) && !c->fn[0])
        return rlrc;
    else if(!rlrc)
        esccpy(c->fn, buf, ESCCHAR, ESCCHAR, MBCH);

    if(randstr(buf, UCLEN)) strncpy(c->uid, buf, UCLEN);

    c->lid = getindex(db, verb) + 1;

    if(!readline("Organization", buf, c->org, ORLEN))
        esccpy(c->org, buf, ESCCHAR, ESCCHAR, MBCH);

    if((rlrc = mknewphone(c, prompt, buf))) {
        fprintf(stderr, "Error: Incorrect phone number format\n");
        return rlrc;
    }

    if((rlrc = mknewemail(c, prompt, buf))) {
        fprintf(stderr, "Error: Incorrect email address format\n");
        return rlrc;
    }

    free(buf);
    free(prompt);

    return 0;
}

// Deletes card c from the database
static int delcard(sqlite3 *db, card *c, const flag *f) {

    int last = getindex(db, f->vfl);
    char *sql = calloc(BBCH, sizeof(char)), *err = 0;

    snprintf(sql, BBCH, "DELETE FROM id WHERE lid=%d;", c->lid);
    int dbrc = sqlite3_exec(db, sql, 0, 0, &err);

    if(c->lid != last) mvcard(db, last, c->lid);

    if(f->vfl) {
        printf("Deleted card:\n");
        printcard(c, f);
    }

    free(sql);
    return dbrc;
}

// Wrapper for icard()
static int iloop(sqlite3 *db, const flag *f, const char *fname) {

    FILE *vf = fopen(fname, "r");
    card *c = calloc(1, sizeof(card));

    int dbrc = 0;

    icard(c, vf, db, f->vfl);
    if(!valcard(c)) dbrc = wrcard(db, c, f->op, f->vfl);
    else return 1;

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
                    if(f->vfl) printf("[%d] %s imported\n", ctr, fpath);
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

// Guess organization name from email address
static int orgfromemail(card *c) {

    int len = strlen(c->em[0]);
    if(len < 1) return 1;

    char *p1 = calloc(len, sizeof(char));
    char *p2 = p1;

    unsigned int a = 0, sep = len;

    while(c->em[0][sep] != '.') sep--;
    memcpy(p2, c->em[0], sep);

    len = strlen(p2);
    for(a = 0; a < len; a++) { if(p2[a] == '.' || p2[a] == '@') sep = a; }
    p2 += sep + 1;

    p2[0] = toupper(p2[0]);
    strncpy(c->org, p2, ORLEN);

    free(p1);
    return 0;
}

// Read card data from raw email dump
static int rawread(card *c, const flag *f) {

    char *buf = calloc(BBCH, sizeof(char));
    char *nbuf = calloc(BBCH, sizeof(char));

    unsigned int a = 0, b = 0, isn = 1;

    while(fgets(buf, BBCH, stdin)) {
        if(!strst(buf, FRKEY)) {
            strncpy(nbuf, robj(buf, BBCH), BBCH);
            int slen = strlen(nbuf);
            for(a = 0; a < slen; a++) {
                if(nbuf[a] == '<') b = isn = 0;
                else if(nbuf[a] == '>') break;
                else if(isn) c->fn[b++] = nbuf[a];
                else c->em[0][b++] = nbuf[a];
            }

        }
    }

    free(buf);
    free(nbuf);

    return 0;
}

// Check if name is already in db
static int nameindb(sqlite3 *db, const card *c, card *cmp, const flag *f) {

    char sql[BBCH];
    snprintf(sql, BBCH, "SELECT * FROM id WHERE fn LIKE '%%%s%%';", c->fn);
    return searchdb(db, cmp, f, sql, 0);
}

// Prompt user to merge cards
static int joinprompt(card *c1, card *c2, const flag *f) {

    char ans[3];

    printcard(c1, f);
    printcard(c2, f);

    readline("Merge (y/n)", ans, "n", 3);
    
    if(tolower(ans[0]) == 'y') return 1;
    else return 0;
}

// Wrapper for joincard()
static int joinwr(sqlite3 *db, card *c1, card *c2, const flag *f) {

    char sql[BBCH];

    if(!c1->fn[0]) {
        snprintf(sql, BBCH, "SELECT * FROM id WHERE lid=%d;", c1->lid);
        searchdb(db, c1, f, sql, 0);
    }

    if(!c2->fn[0]) {
        snprintf(sql, BBCH, "SELECT * FROM id WHERE lid=%d;", c2->lid);
        searchdb(db, c2, f, sql, 0);
    }

    joincard(c1, c2);
    delcard(db, c1, f);
    delcard(db, c2, f);
    wrcard(db, c1, f->op, f->vfl);

    return 0;
}

// Execute create operation
static int exec_create(sqlite3 *db, const flag *f, const char *dbpath) {

    if(ctable(db)) return usage("Could not create/reset database", f);
    else printf("Database %s created successfully\n", dbpath);

    return 0;
}

// Execute import operation
static int exec_import(sqlite3 *db, const flag *f, char **args,
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
                if(f->vfl) printf("[%d] %s imported\n", ctr, args[a]);
            }
        }
    }

    if(f->vfl) printf("%d File(s) imported successfully\n", ctr);

    free(fpath);
    return 0;
}

// Execute join operation
// TODO: refactor and clean up;
// transform to take linked list and extend to merge multiple cards
static int exec_join(sqlite3 *db, const flag *f, char **args,
    const int numarg) {

    int last = getindex(db, f->vfl);

    card *c1 = calloc(1, sizeof(card));
    card *c2 = calloc(1, sizeof(card));

    if(numarg == 2) {
        c1->lid = matoi(args[0]);
        c2->lid = matoi(args[1]);
    }

    if(numarg != 2 || c1->lid == c2->lid ||
        c1->lid > last || c1->lid < 1 ||
        c2->lid > last || c2->lid < 1) {
        return usage("Specify two LID to join cards", f);

    } else {
        joinwr(db, c1, c2, f);
        free(c1);
        free(c2);
        return 0;
    }
}

// Execute export operation
static int exec_export(sqlite3 *db, const flag *f, char **args,
    const int numarg) {

    char *sstr = calloc(BBCH, sizeof(char));
    char *tstr = calloc(BBCH, sizeof(char));
    char *fpath = calloc(MBCH, sizeof(char));

    card *head = calloc(1, sizeof(card));
    card *c = head;

    strncpy(sstr, atostr(tstr, args, numarg), BBCH);
    mkdeck(db, c, f, sstr);

    c = head;

    while(c->lid) {
        ecard(c, f, fpath);
        if(f->vfl) printf("%s exported as: %s\n", c->fn, fpath);
        if(c->next) c = c->next;
    }

    free(sstr);
    free(tstr);
    free(fpath);
    return 0;
}

// Execute 'new' operation
// TODO: Refactor and clean up
static int exec_new(sqlite3 *db, const flag *f, char **args, const int anum) {

    int dbrc = 0, cci = 0;

    card *c = calloc(1, sizeof(card));
    card *cmp = calloc(1, sizeof(card)), *head = cmp;

    c = setcarddef(c, args, anum);

    if(mknew(db, c, f->vfl)) return 1;
    cci = nameindb(db, c, cmp, f);

    if(!valcard(c)) {
        if((dbrc = wrcard(db, c, f->op, f->vfl)))
            printf("SQL Error #: %d\n", dbrc);

        else if (cci) {
            cmp = head;
            printf("%s already exists in database. %d merge%spossible.\n",
                   c->fn, cci, cci == 1 ? " " : "s ");
        
            while(cmp->lid) {
                if(joinprompt(c, cmp, f)) joinwr(db, c, cmp, f);
                if(cmp->next) cmp = cmp->next;
                else break;
            }
        }

        if(f->vfl) printcard(c, f);

    } else {
        return usage("Invalid contact format", f);
    }

    free(c);
    return 0;
}

// Execute delete operation
static int exec_delete(sqlite3 *db, const flag *f, char **args) {

    card *head = calloc(1, sizeof(card));
    card *ccard = head;

    mkdeck(db, ccard, f, args[0]);

    if(!valcard(ccard)) delcard(db, head, f);
    else fprintf(stderr, "Found no contact with ID #%d.\n", matoi(args[0]));

    free(head);

    return 0;
}

// Execute 'raw' operation TODO: better handling of return values
static int exec_raw(sqlite3 *db, const flag *f) {

    card *c = calloc(1, sizeof(card));

    int ret = rawread(c, f);
    orgfromemail(c);

    if(c->fn[0]) remtchar(c->fn, ' ');
    if(!ret) ret = mknew(db, c, f->vfl);

    if(!ret && !valcard(c)) {
        if((ret = wrcard(db, c, f->op, f->vfl)))
            printf("SQL Error #: %d\n", ret);
        else if(f->vfl)
            printcard(c, f);
    }

    return ret;
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

// Execute operations
static int execute(sqlite3 *db, const flag *f, const char *dbpath,
    const int alen, char **args) {

    errno = 0;

    switch(f->op) {

        case help:
            return usage("", f);

        case create:
            return exec_create(db, f, dbpath);

        case import:
            return exec_import(db, f, args, alen);

        case join:
            return exec_join(db, f, args, alen);

        case export:
            return exec_export(db, f, args, alen);

        case new:
            return exec_new(db, f, args, alen);

        case delete:
            return exec_delete(db, f, args);

        case raw:
            return exec_raw(db, f);

        default:
            return exec_search(db, f, args, alen);
    }
}

// Set all flags to default values
static void iflag(flag *f, const char *cmd) {

    strncpy(f->cmd, cmd, CMDLEN);

    f->op = -1;
    f->mxnum = NUMCARD;
    f->vfl = 0;
    f->sfl = -1;
}

int main(int argc, char **argv) {

    char *dbpath = calloc(MBCH, sizeof(char));

    flag *f = calloc(1, sizeof(flag));

    sqlite3 *db;

    int ret = 0, optc;

    srandom(time(NULL));
    iflag(f, basename(argv[0]));

    while((optc = getopt(argc, argv, "d:f:hn:v")) != -1) {
        switch(optc) {

            case 'd':
                strncpy(f->dfl, optarg, MBCH);
                break;

            case 'f':
                strncpy(dbpath, optarg, MBCH);
                break;

            case 'h':
                return usage("", f);
                break;

            case 'n':
                f->mxnum = matoi(optarg);
                if(f->mxnum > NUMCARD) f->mxnum = NUMCARD;
                else if(f->mxnum < 1)
                    return usage("n cannot be zero or negative", f);
                break;

            case 'v':
                f->vfl++;
                break;

            default:
                return usage("", f);
                break;
        }
    }

    // Open database
    if(!dbpath[0])
        snprintf(dbpath, MBCH, "%s%c%s", getenv("HOME"), DDIV, DBNAME);
    if(sqlite3_open(dbpath, &db)) errno = ENOENT;
    else errno = 0;

    // Dump DB if no argument has bee
     if(argc <= optind && !errno) {
        f->op = all;
        ret = exec_search(db, f, NULL, 0);

        sqlite3_close(db);
        return ret;

    // Create database if it doesn't exist
    } else if(errno) {
        exec_create(db, f, dbpath);
        errno = 0;
        if(argc <= optind) return 0;
    }

    // Set and verify operation
    f->op = chops(argv[optind], SBCH);
    if(f->op < 0) errno = EINVAL;
    if(f->op == delete) f->sfl = lid;
    if(f->op == join) {
        f->sfl = lid;
    }

    if(errno) return usage("", f);

    // Prepare argument list
    argc -= optind + 1;
    argv += optind + 1;

    // Initiate operations
    ret = execute(db, f, dbpath, argc, argv);

    // Graceful exit
    free(dbpath);
    free(f);
    sqlite3_close(db);
    return ret;
}
