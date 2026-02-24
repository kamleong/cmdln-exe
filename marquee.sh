#!/bin/bash

# The text to scroll
MSGTXT="$1"
[ -z "$MSGTXT" ] && MSGTXT="Welcome to the Shell Script Marquee!"
MSGTXT=`echo -n "$MSGTXT" |sed -z 's/\n/# /g'` # |tr '\n' '|'

# The width of the display area (e.g., terminal width)
COLS=$(tput cols) ; [ -z "$COLS" ] && COLS=80 # get terminal width

tput sc # Save current cursor position
tput cup 0 0 # go to top line
# Restore cursor and exit on <ctrl-c> or <terminal-WINdow-CHange>
trap 'tput rc; exit' SIGINT SIGWINCH

while true; do
  # Pad the message to ensure smooth scrolling off-screen
  PADDING=$(printf '%*s' $COLS)
  SCROLL_MSG="${PADDING}${MSGTXT}${PADDING}"
  SCROLL_MAX=$((${#PADDING} + ${#MSGTXT}))
  for ((i=0; i<$SCROLL_MAX; i++)); do
    ii=$((i + 25)) ## start in the middle 
    if [ $ii -gt $SCROLL_MAX ]; then
      ii=$((ii - $SCROLL_MAX))
    fi
    echo -ne "\r$(tput el)" # Clear current line and move cursor to the beginning
    echo -n "${SCROLL_MSG:$ii:$COLS}"
    sleep 0.15 # scrolldelay 
  done
done

tput rc # Restore cursor to the saved position
