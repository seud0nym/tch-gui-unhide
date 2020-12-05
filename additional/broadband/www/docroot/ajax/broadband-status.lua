local json = require("dkjson")
local proxy = require("datamodel")
local content_helper = require("web.content_helper")

local wan_intf ="wan"
local content_wan = {
  wwan_ipaddr = "rpc.network.interface.@wwan.ipaddr",
  wwan_ip6addr = "rpc.network.interface.@wwan.ip6addr",
}
content_helper.getExactContent(content_wan)
if content_wan.wwan_ipaddr:len() ~= 0 or content_wan.wwan_ip6addr:len() ~= 0 then
  wan_intf = "wwan"
end
local content_rpc = {
  tx_bytes = "rpc.network.interface.@" .. wan_intf .. ".tx_bytes",
  rx_bytes = "rpc.network.interface.@" .. wan_intf .. ".rx_bytes",
}
content_helper.getExactContent(content_rpc)

local data = {
  tx_bytes = content_rpc.tx_bytes,
  rx_bytes = content_rpc.rx_bytes,
  uptime = content_helper.readfile("/proc/uptime","number",floor),
}

local buffer = {}
if json.encode (data, { indent = false, buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
