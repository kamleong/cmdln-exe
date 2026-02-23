#!/bin/sh
[ -z "$1" ] && q="$QUERY_STRING" || q="$1" ; [ -z "$q" ] && q="google.news"
q=`echo "$q" | sed 's/^q=//'`
refresh_interval=90

echo "Content-Type: text/html; charset=utf-8"
echo ""
cat << EOT
 <form>
   <script language="JavaScript">
     var timeoutID = 0, timeout_in_miliseconds = ${refresh_interval} * 1000;
     if ( timeout_in_miliseconds > 0 ) {
       timeoutID = setTimeout("reloadThisPage()", timeout_in_miliseconds);
     }
     function reloadThisPage() {
       window.top.location.reload(); // mainForm.submit();
     }
     function stopAutoRefresh() {
       clearTimeout(timeoutID);
       document.getElementById("autoRefreshDIV").innerHTML = "";
     }
   </script>
   <SPAN id=autoRefreshDIV style="text-align:right">
     <B style="color=IndianRed">
       AutoRefresh
       <select xname="refresh_interval" xonChange="this.form.submit()">
        <option>${refresh_interval}</option>
       </select>s
     </B>
     &nbsp; &nbsp;
     [<A class=href onClick="reloadThisPage()">refresh</A>]
     [<A class=href onClick="stopAutoRefresh()">cancel</A>]
    </SPAN>
    <small><small>$(date)</small></small>
    &nbsp; &nbsp;
    <select name=q onchange='this.form.submit()'>
      <option value="$q">$q</option>
      <option value="news.my">Malay Mail</option>
      <option value="rthk">香港電台</option>
      <option value="bbc.zh">BBC 中文</option>
      <option value="bbc">BBC</option>
      <option value="google.news">Google News</option>
    </select>
    &nbsp;
    <input type=submit value="submit">
  </form>
  <hr>
EOT

[ "$q" == "google.news" ] && q="https://news.google.com/rss?gl=MY"
[ "$q" == "news.my" ] && q="https://www.malaymail.com/feed/rss/"
[ "$q" == "rthk" ] && q="https://rthk.hk/rthk/news/rss/c_expressnews_cinternational.xml"
[ "$q" == "bbc.zh" ] && q="https://feeds.bbci.co.uk/zhongwen/trad/rss.xml"
[ "$q" == "bbc" ] && q="http://feeds.bbci.co.uk/news/rss.xml"
[ "$q" == "" ] && q=""

/root/rss.lua "$q" "HTML"

exit 0
