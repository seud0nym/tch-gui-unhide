local json = require("dkjson")
local proxy = require ("datamodel")

local log,err = proxy.get("rpc.gui.acme.log")
if log then
  log = log[1].value
else
  log = err
end

local data = {
  html = log,
}

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
