local json = require("dkjson")
local ipsec_helper = require("wireguard_helper")
local html = ipsec_helper.getWireguardCardHTML()

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
