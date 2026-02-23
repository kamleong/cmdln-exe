#!/usr/bin/env python3

from datetime import datetime, timezone, timedelta
import xml.etree.ElementTree as ET
import requests
import sys
import os
import re

currtimeLocal = datetime.now()
currtimeUTC = currtimeLocal.astimezone(timezone.utc) ## datetime.now(timezone.utc)
tzoffset = timedelta(hours=8, minutes=0) ## currtimeLocal - currtimeUTC.replace(tzinfo=None)
tz = timezone(tzoffset) ## = pytz.timezone("Asia/Singapore")

currtime = currtimeLocal.astimezone(tz)
#currtime = datetime(2026,1,13, 9,30,0, tzinfo=tz)
#print(str(currtimeLocal) + " [ " + currtime.strftime('%H:%M:%S %z') + " == " + currtimeUTC.strftime('%H:%M:%S %z') + " ] " + str(currtime-currtimeUTC))
#quit()

opt = '' if (len(sys.argv)<3) else sys.argv[2]
url = 'https://news.google.com/rss?gl=MY' if (len(sys.argv)<2) else sys.argv[1]
resp = requests.get(url)
resp.raise_for_status()

#tree = ET.parse('rss.xml'); root = tree.getroot()
root = ET.fromstring(resp.content)
i = 0; rssPubDate = {}; rssTitle = {}; rssUrl = {}
#for p in root.findall('.//programme[@channel="677fb08f5b68381e2dbfd5af"]'):
for p in root.findall('.//item'):
  i = i + 1
  pubDate = p.find('pubDate').text
  #print(pubDate) ; break;
  tzFormat = '%z' if re.search('\+\d{4}$', pubDate) else '%Z'
  pubDate = datetime.strptime(pubDate, '%a, %d %b %Y %H:%M:%S '+tzFormat).astimezone(tz)
  #print(pubDate) ; break;
  rssPubDate[i] = pubDate
  rssTitle[i] = p.find('title').text
  rssUrl[i] = p.find('link').text
  #print(
  #  pubDate.strftime('%Y-%m-%d %H:%M:%S') + " > " +
  #  title
  #)
  #print(ET.tostring(p, encoding='utf-8'))
  #break
#

rssPubDate = {k: v for k, v in sorted(rssPubDate.items(), key=lambda item: item[1], reverse=True)}
print("last [pubDate] " + str(next(iter(rssPubDate.values()))))

maxline = 100 ; maxlen = 300

if opt=="HTML" :
  print("*HTML*<br><br>")
else :
  termsz = os.get_terminal_size(0)
  ## specify fd=sys.stdin.fileno() explicitly above for compatibility with piping and watch command, to avoid OSError: [Errno 25] Inappropriate ioctl for device
  maxline = termsz.lines-2 ; maxlen = termsz.columns-1
#

i = 0
for key, val in rssPubDate.items():
#for key in rss_items.keys():
  i = i + 1 ; pubDate = val
  if opt=="HTML" :
    print(
      #pubDate.strftime('%H:%M:%S') +
      "<li style='list-style-type:none;padding-left:15px;text-indent:-15px;' xonclick=\"window.open('" + rssUrl[key] + "')\">"
      + "<a target='_blank' href='" + rssUrl[key] + "'><small><small>&gt;</small></small></a> "
      + rssTitle[key]
    )
  else :
    print(
      #pubDate.strftime('%H:%M:%S') +
      ">" + rssTitle[key][:maxlen]
    )
    if i >= maxline : break
  #
  #print(currtime) ; #print(pubDate)
  #break
  if (currtime-pubDate) > timedelta(hours=18) : break
#
