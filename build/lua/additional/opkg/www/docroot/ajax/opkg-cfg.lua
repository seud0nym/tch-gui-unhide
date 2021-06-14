local json = require("dkjson")
local proxy = require ("datamodel")

local last_update, err = proxy.get("rpc.gui.opkg.last_update")
if last_update then
  if last_update[1].value == "-1" then
    last_update = T"Not since last reboot"
  else
    last_update = T(os.date("%d/%m/%Y %H:%M:%S", tonumber(last_update[1].value)))
  end
  else
  last_update = err
end

local log, err = proxy.get("rpc.gui.opkg.log")
if log then
  log = log[1].value
else
  log = err
end

local data = {
  text = log,
  updated = last_update,
}

local buffer = {}
if json.encode (data, { indent = false, buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
