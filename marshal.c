#include "iddb.h"

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

    mstr[w] = 0;
    return mstr;
}

// Return marshalled string to array format
char **unmarshal(const char *str, char **arr) {

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
