local json = require("dkjson")
local content_helper = require("web.content_helper")
local bbch = require("broadbandcard_helper")

local wan_data = {
  wans_enable = "uci.wansensing.global.enable",
}
content_helper.getExactContent(wan_data)

local html = bbch.getBroadbandCardHTML(wan_data.wans_enable)

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
