local json = require("dkjson")
local ssid_helper = require("ssid_helper")

local html = ssid_helper.getWiFiCardHTML()

local data = {
  html = table.concat(html,"\n"),
}

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
