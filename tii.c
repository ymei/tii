#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tii.h"

int tii_parse_env(char **host, char **port)
{
    char *envValue, buf[ENV_VAL_MAXLEN], *p, *pe;
    int len, hlen, plen, ret=0;

    if((envValue = getenv(ENV_VAR_NAME)) == NULL) { /* env not defined, use default value */
        ret = 1;
        len = strlen(ENV_VAR_DEFAULT);
        strncpy(buf, ENV_VAR_DEFAULT, sizeof(buf));
        setenv(ENV_VAR_NAME, ENV_VAR_DEFAULT, 1);
    } else {
        ret = 0;
        len = strlen(envValue);
        strncpy(buf, envValue, sizeof(buf));
    }

    if(len >= ENV_VAL_MAXLEN) {/* environment string too long, return error */
        fprintf(stderr, "Environment variable env(TIIADDR) length exceeds %d\n", ENV_VAL_MAXLEN);
        return (ret = -1);
    }

    for(p = buf; (p-buf <= len) && isblank(*p); p++) {;}
    for(pe = p; (pe-buf <= len) && *pe != ':'; pe++) {;}
    if(pe-buf >= len) { /* no ':' found */
        fprintf(stderr, "No `:' found in env(TIIADDR).\n");
        return (ret = -1);
    }

    hlen = pe - p;
    *host = (char*)calloc(hlen + 1, sizeof(char)); /* +1 for '\0' termination */
    strncpy(*host, p, hlen);

    pe++;
    plen = len-hlen-1;
    *port = (char*)calloc(plen + 1, sizeof(char));
    strncpy(*port, pe, plen);

    return ret;
}

#ifdef TII_DEBUG_ENABLE_MAIN
int main(int argc, char **argv)
{
    char *host=NULL, *port=NULL;
    int ret;

    ret = tii_parse_env(&host, &port);
    printf("ret = %d\n", ret);
    printf("host = \"%s\"\n", host);
    printf("port = \"%s\"\n", port);

    free(host);
    free(port);
    return EXIT_SUCCESS;
}
#endif
