local json = require("dkjson")
local wanservicescard_helper = require("wanservicescard_helper")
local html = wanservicescard_helper.getWANServicesCardHTML()

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
