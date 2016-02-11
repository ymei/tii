/*
 * Copyright (c) 2011 - 2016
 *
 *     Yuan Mei
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * tiic ip port
 */

/* waitpid on linux */
#include <sys/types.h>
#include <sys/wait.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>

#ifdef __linux /* on linux */
#include <pty.h>
#include <utmp.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#else /* defined(__APPLE__) && defined(__MACH__) */
#include <util.h>
#endif

#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tii.h"

#define error_printf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

static int sfd;
static struct termios tt;
static int isTty;

static void reset_and_exit(int status)
{
    if(isTty) /* recover the controlling terminal's original state */
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &tt);
    if(sfd>=0)
        close(sfd);
    exit(status);
}

#define MAXSLEEP 4
static int connect_retry(int sockfd, const struct sockaddr *addr, socklen_t alen)
{
    int nsec;
    /* Try to connect with exponential backoff. */
    for (nsec = 1; nsec <= MAXSLEEP; nsec <<= 1) {
        if (connect(sockfd, addr, alen) == 0) {
            /* Connection accepted. */
            return(0);
        }
        /*Delay before trying again. */
        if (nsec <= MAXSLEEP/2)
            sleep(nsec);
    }
    return(-1);
}

static int get_socket(char *host, char *port)
{
    int status;
    struct addrinfo addrHint, *addrList, *ap;
    int sockfd, sockopt;

    memset(&addrHint, 0, sizeof(struct addrinfo));
    addrHint.ai_flags = AI_CANONNAME|AI_NUMERICSERV;
    addrHint.ai_family = AF_INET; /* we deal with IPv4 only, for now */
    addrHint.ai_socktype = SOCK_STREAM;
    addrHint.ai_protocol = 0;
    addrHint.ai_addrlen = 0;
    addrHint.ai_canonname = NULL;
    addrHint.ai_addr = NULL;
    addrHint.ai_next = NULL;

    status = getaddrinfo(host, port, &addrHint, &addrList);
    if(status < 0) {
        error_printf("getaddrinfo: %s\n", gai_strerror(status));
        return status;
    }

    for(ap=addrList; ap!=NULL; ap=ap->ai_next) {
        sockfd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
        if(sockfd < 0) continue;
        sockopt = 1;
        if(setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &sockopt, sizeof(sockopt)) == -1) {
            close(sockfd);
            warn("setsockopt");
            continue;
        }
        if(connect_retry(sockfd, ap->ai_addr, ap->ai_addrlen) < 0) {
            close(sockfd);
            warn("connect");
            continue;
        } else {
            break; /* success */
        }
    }
    if(ap == NULL) { /* No address succeeded */
        error_printf("Could not connect, tried %s:%s\n", host, port);
        return -1;
    }
    freeaddrinfo(addrList);
    return sockfd;
}

int main(int argc, char **argv)
{
    struct termios ttt;
    struct winsize win;
    char *host=NULL, *port=NULL;
    int maxfd = 2;
    fd_set rfd;
    char ibuf[BUFSIZ];
    int isReadingStdin, nsel, nrw;

    if(tii_parse_env(&host, &port) < 0) {
        fprintf(stderr, "Environment variable parse error.\n");
        return EXIT_FAILURE;
    }

    sfd = get_socket(host, port);
    if(sfd < 0) {
        fprintf(stderr, "Failed to establish a socket.\n");
        return EXIT_FAILURE;
    }
    if(host) {free(host); host = NULL;}
    if(port) {free(port); port = NULL;}

    if(sfd > maxfd) maxfd = sfd;

    if((isTty = isatty(STDIN_FILENO)) != 0) {
        /* if invoked under a true tty, save the parameters of the controlling terminal
         */
        if(tcgetattr(STDIN_FILENO, &tt) == -1)
            err(1, "tcgetattr");
        if(ioctl(STDIN_FILENO, TIOCGWINSZ, &win) == -1)
            err(1, "ioctl");
    }

    if(isTty) { /* set the controlling terminal to raw mode, no echo. */
        ttt = tt;
        cfmakeraw(&ttt);
        ttt.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &ttt);
    }

    isReadingStdin = 1;
    for(;;) {
        FD_ZERO(&rfd);
        FD_SET(sfd, &rfd);
        if(isReadingStdin)
            FD_SET(STDIN_FILENO, &rfd);
        if(!isReadingStdin && isTty) {
            isReadingStdin = 1;
        }
        nsel = select(maxfd + 1, &rfd, NULL, NULL, NULL);
        if(nsel < 0 && errno != EINTR)
            break;
        /* if something is coming from the network */
        if(nsel > 0 && FD_ISSET(sfd, &rfd)) {
            nrw = read(sfd, ibuf, sizeof(ibuf));
            if(nrw < 0)
                break;
            if(nrw == 0) { /* network is closed (probably) */
                break;
            }
            (void)write(STDOUT_FILENO, ibuf, nrw);
        }
        /* if something is typed into the controlling terminal */
        if(nsel > 0 && FD_ISSET(STDIN_FILENO, &rfd)) {
            nrw = read(STDIN_FILENO, ibuf, sizeof(ibuf));
            if(nrw < 0)
                break;
            if(nrw == 0) { /* EOF */
                isReadingStdin = 0;
                break;
            }
            if(nrw > 0) {
                (void)write(sfd, ibuf, nrw);
            }
        }
    }

    reset_and_exit(EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
