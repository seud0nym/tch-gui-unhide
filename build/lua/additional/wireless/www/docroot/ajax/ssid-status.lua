local json = require("dkjson")
local ssid_helper = require("ssid_helper")
local ssid_list = ssid_helper.getSSIDList()

local data = {}
  for i,v in ipairs(ssid_list) do
  if i <= 5 then
    data[v.id] = v.state or "0"
  end
end

local buffer = {}
if json.encode (data, { indent = false, buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
