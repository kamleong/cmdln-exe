#!/usr/bin/env lua

local function toLocalTime(s)
  -- s = "Sat, 29 Oct 1994 19:43:31 GMT"
  local p = "%a+, (%d+) (%a+) (%d+) (%d+):(%d+):(%d+) *(%S*)"
  local dd,mmm,yyyy,hh,mi,ss,tz = s:match(p)
  local months = {Jan=1,Feb=2,Mar=3,Apr=4,May=5,Jun=6,Jul=7,Aug=8,Sep=9,Oct=10,Nov=11,Dec=12}
  local mm = months[mmm]
  local tzOffset = 0 -- ; print(tz)
  if tz==nil or tz=="" then
    tzOffset = 0
  else
    local localTime = os.time()
    local localTzOffset = localTime - os.time(os.date("!*t", localTime))
    -- print(localTzOffset, os.date("%c",localTime), "[UTC] = " .. os.date("!%c",localTime))
    if tz=="GMT" then
      tzOffset = localTzOffset
    else
      p = "(%S)(%d+):?(%d+)"
      local o,h,m = tz:match(p) -- ; print(o, h, m) ; os.exit()
      tzOffset = tonumber(h)*3600 + tonumber(m)*60
      if o=="-" then tzOffset=-tzOffset end
      tzOffset = tzOffset - localTzOffset
    end
  end
  return os.time({day=dd,month=mm,year=yyyy,hour=hh,min=mi,sec=ss})+tzOffset
end
--print( os.date("%c", toLocalTime("Sat, 29 Oct 1994 19:43:31 GMT")) ) ; os.exit()

local function get_terminal_size()
  local handle = io.popen("stty -F /dev/tty size 2>/dev/null")
  if handle then
    local result = handle:read("*a") ; handle:close()
    if result then
      local rows, cols = result:match("(%d+)%s+(%d+)")
      if rows and cols then return tonumber(cols), tonumber(rows) end
    end
  end
  return 80, 25 -- default
end
local x, y = get_terminal_size() -- ; print("TerminalSize: " .. x .. "," .. y) ; os.exit()

--[[
local http = require("socket.http")
local function get_url_content(url)
  local body, status, headers, err = http.request(url)
  if status == 200 then
    return body
  else
    return nil, err or status
  end
end
]]

local function get_url_content(url)
  local f = io.popen("curl -s -L '" .. url .. "'")
  if f then
    local content = f:read("*a") -- Read the entire output
    f:close()
    return content
  else
    return nil, "Failed to run curl command"
  end
end

local url = (#arg>0) and arg[1] or "https://news.google.com/rss?gl=MY"
local xml = get_url_content(url) -- ; print(xml) ; os.exit()

package.path = "/usr/share/lua/5.1/?.lua;"..package.path
local xml2lua = require("xml2lua")
local handler = require("xmlhandler.tree") -- converts the XML to a Lua table

local parser = xml2lua.parser(handler) ; parser:parse(xml)
--print("lastBuildDate: " .. handler.root.rss.channel.lastBuildDate)

local items = handler.root.rss.channel.item -- ; print("items: " .. #items)
if #items==0 and item.title~=nil then items={[1]=items} end
-- bug? workaround above when a single item is found and data structure is not nested
-- for i, r in pairs(items) do print(">" .. string.sub(r.title, 1, x-1)) end ; os.exit()

local keys={} ; for k, _ in pairs(items) do
  table.insert(keys, k) ; items[k].pubDate = toLocalTime(items[k].pubDate)
end
table.sort(keys,
  function(a, b) return items[a].pubDate > items[b].pubDate end
)
--print("lastPubDate: [" .. items[keys[1]].pubDate .. "] ".. items[keys[1]].title); os.exit()

print("lastPubDate: " .. os.date("%c",items[keys[1]].pubDate))
local i = 0 ; y = y-2
for _, k in ipairs(keys) do
  i = i + 1
  print(">" .. string.sub(items[k].title, 1, x-1))
  if i >= y then break end
end
