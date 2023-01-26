#!/bin/sh

if [ -z "$USE_XTERM" ]; then
    export USE_XTERM=0
fi

if [ -z "$TIIADDR" ]; then
    export TIIADDR="localhost:7115"
fi

if [ $(uname) = "Darwin" ] && [ $USE_XTERM = 0 ]; then
    osascript 2>/dev/null <<EOF
      tell application "System Events"
        tell process "Terminal" to keystroke "n" using command down
      end
      tell application "Terminal"
        activate
        do script "tiis gnuplot" in selected tab of the front window
      end tell
EOF
else
    xterm -e tiis gnuplot &
fi

sleep 1
exec tiic
