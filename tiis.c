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
 * tiis gnuplot arg0 arg1 arg2 ...
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

static int ptyMaster, ptySlave;
static int afd;
static pid_t childPid;
static struct termios tt;
static int isTty;

static void reset_and_exit(int status)
{
    if(isTty) /* recover the controlling terminal's original state */
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &tt);
    close(afd);
    exit(status);
}

static void start_slave_process(int argc, char **argv)
{
    char **av;
    int i;

    av = (char**)calloc(argc, sizeof(char*));
    for(i=0; i<argc-1; i++)
        av[i] = argv[i+1];

    close(ptyMaster);
    login_tty(ptySlave);

    execvp(av[0], av);
    /* execlp("gnuplot", "gnuplot", (char*)NULL); */
    warn("%s", av[0]);
    
    /* reach here only when exec fails */
    kill(0, SIGTERM); /* kill the whole process group */
    reset_and_exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    struct termios ttt, stt;
    struct winsize win;
    int maxfd = 2;
    fd_set rfd;
    int isReadingStdin, isAnyHost, nsel, nrw, status, errorCode;
    char ibuf[BUFSIZ], obuf[BUFSIZ]; /* BUFSIZ is defined in stdio.h, see man setbuf */
    char *host=NULL, *port=NULL;
    struct addrinfo addrHint, *addrList, *ap;
    struct sockaddr caddr;
    socklen_t caddrLen;
    int sockfd, sockopt;
    
    if(tii_parse_env(&host, &port) < 0) {
        fprintf(stderr, "Environment variable parse error.\n");
        return EXIT_FAILURE;
    }
    isAnyHost = 0;
    if(host[0]=='\0' || host[0]=='*') { /* if host is empty or is '*',
                                         * we accept connection to any
                                         * host ip address */
        isAnyHost = 1;
        free(host);
        host = NULL;
    }

    memset(&addrHint, 0, sizeof(struct addrinfo));
    addrHint.ai_flags = AI_CANONNAME|AI_NUMERICSERV;
    if(isAnyHost) addrHint.ai_flags |= AI_PASSIVE;
    addrHint.ai_family = AF_INET; /* we deal with IPv4 only, for now */
    addrHint.ai_socktype = SOCK_STREAM;
    addrHint.ai_protocol = 0;
    addrHint.ai_addrlen = 0;
    addrHint.ai_canonname = NULL;
    addrHint.ai_addr = NULL;
    addrHint.ai_next = NULL;

    status = getaddrinfo(host, port, &addrHint, &addrList);
    if(status < 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return EXIT_FAILURE;
    }

    for(ap=addrList; ap!=NULL; ap=ap->ai_next) {
        sockfd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
        if(sockfd < 0) continue;
        /* set SO_REUSEADDR so that when the socket is closed and is in TIME_WAIT
         * status, bind() can still succeed.
         */
        sockopt = 1;
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) == -1) {
            close(sockfd);
            err(1, "setsockopt");
        }
        if(bind(sockfd, ap->ai_addr, ap->ai_addrlen) == 0) {
            if(listen(sockfd, 1) < 0)
                err(1, "listen");
            if(inet_ntop(AF_INET, &(((struct sockaddr_in *)(ap->ai_addr))->sin_addr),
                         obuf, sizeof(obuf)) == NULL) {
                warn("inet_ntop");
            }
            printf("Listening on host \"%s\" (%s), port %d\n", ap->ai_canonname,
                   obuf,
                   ntohs(((struct sockaddr_in *)(ap->ai_addr))->sin_port));
            break; /* success */
        }
        /* if bind() fails */
        close(sockfd);
    }

    if(ap==NULL) { /* No address succeeded */
        fprintf(stderr, "Could not bind, tried %s:%s\n", host, port);
        return EXIT_FAILURE;
    }

    freeaddrinfo(addrList);
    if(host) {free(host); host = NULL;}
    if(port) {free(port); port = NULL;}

    caddrLen = sizeof(caddr);
    if((afd = accept(sockfd, &caddr, &caddrLen)) < 0)
        err(1, "accept");
    if(sockfd > maxfd) maxfd = sockfd;
    if(afd > maxfd) maxfd = afd;
    close(sockfd); /* we only allow one connection */

    if((isTty = isatty(STDIN_FILENO)) != 0) {
        /* if invoked under a true tty, the slave pty is allocated
         * with the terminal parameters of the controlling terminal
         */
        if(tcgetattr(STDIN_FILENO, &tt) == -1)
            err(1, "tcgetattr");
        if(ioctl(STDIN_FILENO, TIOCGWINSZ, &win) == -1)
            err(1, "ioctl");
        stt = tt;
        // stt.c_lflag &= ~ECHO;
        // stt.c_lflag |= ECHONL;
        if(openpty(&ptyMaster, &ptySlave, NULL, &stt, &win) == -1)
            err(1, "openpty");
    } else {
        if(openpty(&ptyMaster, &ptySlave, NULL, NULL, NULL) == -1)
            err(1, "openpty");
    }
    if(ptySlave > maxfd) maxfd = ptySlave;
    if(ptyMaster > maxfd) maxfd = ptyMaster;

    if(isTty) { /* set the controlling terminal to raw mode, no echo. */
        ttt = tt;
        cfmakeraw(&ttt);
        ttt.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &ttt);
    }

    childPid = fork();
    if(childPid < 0) {
        warn("fork");
        reset_and_exit(EXIT_FAILURE);
    }
    if(childPid == 0) {
        start_slave_process(argc, argv);
    }
    close(ptySlave);

    isReadingStdin = 1;
    for(;;) {
        FD_ZERO(&rfd);
        FD_SET(afd, &rfd);
        FD_SET(ptyMaster, &rfd);
        if(isReadingStdin)
            FD_SET(STDIN_FILENO, &rfd);
        if(!isReadingStdin && isTty) {
            isReadingStdin = 1;
        }
        nsel = select(maxfd + 1, &rfd, NULL, NULL, NULL);
        if(nsel < 0 && errno != EINTR)
            break;
        /* if something is coming from the network */
        if(nsel > 0 && FD_ISSET(afd, &rfd)) {
            nrw = read(afd, ibuf, sizeof(ibuf));
            if(nrw < 0)
                break;
            if(nrw == 0) { /* network is closed (probably) */
                ;
            }
            (void)write(ptyMaster, ibuf, nrw);
            // (void)write(STDOUT_FILENO, ibuf, nrw);
        }
        /* if something is typed into the controlling terminal */
        if(nsel > 0 && FD_ISSET(STDIN_FILENO, &rfd)) {
            nrw = read(STDIN_FILENO, ibuf, sizeof(ibuf));
            if(nrw < 0)
                break;
            if(nrw == 0) {
                if(tcgetattr(ptyMaster, &stt) == 0 && (stt.c_lflag & ICANON) != 0) {
                    (void)write(ptyMaster, &stt.c_cc[VEOF], 1);
                }
                isReadingStdin = 0;
            }
            if(nrw > 0) {
                (void)write(ptyMaster, ibuf, nrw);
            }
        }
        /* if something is fed back from the slave */
        if(nsel > 0 && FD_ISSET(ptyMaster, &rfd)) {
            nrw = read(ptyMaster, obuf, sizeof(obuf));
            if(nrw <= 0)
                break;
            (void)write(STDOUT_FILENO, obuf, nrw);
            (void)write(afd, obuf, nrw);
        }
    }

    if(waitpid(childPid, &status, 0) == childPid) {
        if(WIFEXITED(status))
            errorCode = WEXITSTATUS(status);
        else if(WIFSIGNALED(status))
            errorCode = WTERMSIG(status);
        else /* can't happen */
            errorCode = EXIT_FAILURE;
        reset_and_exit(errorCode);
    }

    /* should not reach here but it is safe to assume the clean-up */
    reset_and_exit(EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
