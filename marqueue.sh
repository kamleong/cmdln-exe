#!/bin/bash

# The text to scroll
MESSAGE="Welcome to the Shell Script Marquee!"

# The width of the display area (e.g., terminal width)
# Using 'tput cols' to automatically get terminal width is best
COLS=$(tput cols)

# Fallback if tput is not available
if [ -z "$COLS" ]; then
  COLS=80
fi

# Pad the message to ensure smooth scrolling off-screen
PADDING=$(printf '%*s' $COLS)
SCROLL_MSG="${PADDING}${MESSAGE}${PADDING}"
MSG_LEN=${#SCROLL_MSG}

tput sc # Save current cursor position
tput cup 0 0 # go to top line
# Restore cursor and exit on <ctrl-c> or <terminal-WINdow-CHange>
trap 'tput rc; exit' SIGINT SIGWINCH

while true; do
  for ((i=0; i<$MSG_LEN - $COLS; i++)); do
    #SUBSTR="${SCROLL_MSG:i:$COLS}" # Extract the substring to display
    ii=$((i + 25)) ## let us start in the middle instead
    if [ $ii -gt  $((MSG_LEN - COLS)) ]; then
      ii=$((ii - $((MSG_LEN - COLS))))
    fi
    SUBSTR="${SCROLL_MSG:$ii:$COLS}"
    echo -ne "\r$(tput el)" # Clear current line and move cursor to the beginning
    echo -n "${SUBSTR}"
    sleep 0.1
  done
done

tput rc # Restore cursor to the saved position
