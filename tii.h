#ifndef __TII_H__
#define __TII_H__

#define ENV_VAR_NAME "TIIADDR"
#define ENV_VAR_DEFAULT "localhost:7115" /* default allow connection to localhost */
#define ENV_VAL_MAXLEN 4096

/* get host and port info from environment variable.
 * return -1 when error,
 * return 0 when the ENV is found and used,
 * return 1 when the ENV is not found and the default is used.
 * host and port are malloc-ed, so user is responsible for free-ing the space.
 * port is a string to be convenient for getaddrinfo()
 */
int tii_parse_env(char **host, char **port);

#endif /* __TII_H__ */
