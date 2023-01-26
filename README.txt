NAME
        tii{s,c}

SYNOPSIS

        Allowing a pipe to a child process to be hijacked

        Terminal 1:
                TIIADDR="host:port" tiis program [options]
        Terminal 2:
                TIIADDR="host:port" tiic
        Or see gnuplot_tii.sh to launch a terminal-jailed gnuplot
        in the background while the whole script can be launched
        in popen.

ENVIRONMENT
        TIIADDR.  Defaults to "localhost:7115".

DESCRIPTION

    The name `tii' is inspired by the utility named `tee'.  Instead of
re-routing and duplicating outputs only, `tii' works like a middle-man
that allows interventions on pipes.  The server side program `tiis'
opens a slave process and duplicates its IO to the terminal inside of
which `tiis' is running.  At the same time, `tiis' communicates with
the client side program `tiic' (or another network program such as
telnet or nc), via either network sockets or pipes, and injects what
it receives from the client to the slave process as if the input were
from an interactive session.  `tiis' also sends the response from the
slave process back to the client `tiic', as well as to the terminal
where `tiis' resides.  The client program `tiic' runs in another
terminal (could be on another machine).  From a user's perspective,
interacting with `tiic', or directly typing in the terminal where
`tiis' resides, are the same as directly interacting with the slave
process.

    The whole point of creating such a program is that `tiic' can be
run as a slave (i.e., via popen) to control the desired program (e.g.
gnuplot, in which case gnuplot will be the slave process running under
`tiis').  The communication will be shown in the terminal of `tiis'.
When necessary, human intervention can be typed into the terminal of
`tiis' directly.

    Besides using `tiic', it is equally convenient to connect to
`tiis' host:port directly via TCP, from any language/program.  Instead
of using `popen', a network socket is used to control the slave
process and the message flow is shown in the terminal of `tiis' and
can be altered manually.

    Environment variable `TIIADDR' controls the host and port to
listen on (or to connect to).  TIIADDR="localhost:7115" is the default
which means it listens on localhost at port 7115.  If TIIADDR=":7115"
or "*:7115", it will listen on any ip address on the host.  `7115'
graphically looks like `tiis'.
