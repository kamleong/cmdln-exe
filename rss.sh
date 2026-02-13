#!/bin/sh

alias stty_sz='stty -F /dev/tty size' #! without -F, (true|stty size) will fail with ['standard input': Not a tty]
#alias tput_lines='stty_sz |awk "{print int(\$1-1)}"'
#function stty_sz() {
#  stty -F /dev/tty size 2>/dev/null
#  if [ $? -ne 0 ]; then
#    [ -z "$STTY_SZ0" ] &&  echo "10 40" || echo "$STTY_SZ0"
#  fi
#}
#STTY_SZ0=`stty_sz`
function tput_lines { stty_sz |awk '{print int($1-1)}'; }
tput_cols() echo `stty_sz |cut -d' ' -f2`
tput_cols2() { tput_cols |awk '{print int($1*1.44)}'; } ## echo "`tput_cols`*1.47" |bc

function _rss() {
  url="$1" ; opt="$2" ; x="$3" ; y="$4" ; [ -z "$y" ] && y=`tput_lines`
  [ -z "$1" ] && url="https://news.google.com/rss?gl=MY" ; txt=`curl -s -L "$url"`
  echo "$txt" |sed -n -r 's/^.+lastBuildDate>([^<]+).+$/[lastBuildDate]: \1/p'
  [ -z "$2" ] && txt=`echo "$txt" |tr "<" "\n" |grep -m $y "^title>" |sed "s/^title//"`
  [ "$2" == "cdata" ] && txt=`echo "$txt" |grep -m $y "<title><!" |sed "s/.*CDATA\\[/>/" |sed "s/\\].*//"`
  [ -z "$x" ] && x=`tput_cols` ; [ "$x" == "-" ] && echo "$txt" || echo "$txt" |cut -c "1-$x"
}; rss() ([ -z "$1" ] && _watch -n 20 _rss || _rss "$1" "$2" "$3" "$4")

alias bbc='_watch -n 20 rss "http://feeds.bbci.co.uk/news/rss.xml" "cdata"'
alias cna='rss "https://www.channelnewsasia.com/api/v1/rss-outbound-feed?_format=xml"'

#alias rthk='rss "https://rthk.hk/rthk/news/rss/c_expressnews_cinternational.xml" "cdata" - |awk "{print substr(\$0,1,`tput_cols2`)}"' # busybox awk-substr does not support multibyte chars!
rthk() rss "https://rthk.hk/rthk/news/rss/c_expressnews_cinternational.xml" "cdata" `tput_cols2`
#alias rthk_w="watch 'sh -c \". ~/.profile; rthk\"'" ## openwrt-watch simply not supporting multibyte characters!
alias rthk_w='_watch -n 20 rthk'

alias bbcc='rss "https://feeds.bbci.co.uk/zhongwen/trad/rss.xml" "cdata" `tput_cols2`' # cut-c does not support multibyte chars!
alias news.my='rss "https://www.malaymail.com/feed/rss/" "cdata"'
