local json = require("dkjson")
local bbch = require("broadbandcard_helper")

local html = bbch.getBroadbandCardHTML()

local data = {
  html = table.concat(html, "\n"),
}

local buffer = {}
if json.encode (data, { indent = false, buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
