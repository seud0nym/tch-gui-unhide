local json = require("dkjson")
local gch = require("gatewaycard_helper")

local data = gch.getGatewayCardData()

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
