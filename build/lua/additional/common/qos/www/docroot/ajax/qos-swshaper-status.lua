local json = require("dkjson")
local proxy = require("datamodel")

local content_helper = require("web.content_helper")
local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local shape_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-swshaper-modal.lp" data-id="qos-modal"'

local dataQoS = {
  numShapers = "uci.qos.swshaperNumberOfEntries",
}
content_helper.getExactContent(dataQoS)

local shapers = {
  count = tonumber(dataQoS.numShapers),
  enabled = 0,
  active = 0,
  light = "0",
  names = {},
}
if not shapers.count then
  shapers.count = 0
else
  local swshapers = proxy.getPN("uci.qos.swshaper.",true)
  for _,v in ipairs(swshapers) do
    local name = match(v.path,"uci%.qos%.swshaper%.@([^%.]+)%.")
    shapers.names[name] = untaint(proxy.get(v.path.."enable")[1].value)
    if shapers.names[name] == "1" then
      shapers.enabled = shapers.enabled + 1
    end
  end
end

local ifs = require("qosdevice_helper").getNetworkDevices()
local devices = proxy.getPN("uci.qos.device.",true)
for _,v in ipairs(devices) do
  local device = untaint(match(v.path,"uci%.qos%.device%.@([^%.]+)%."))
  if ifs[device] and ifs[device] ~= "" and ifs[device] ~= "ppp" and ifs[device] ~= "ipoe" then
    local shaper = proxy.get(v.path.."swshaper")[1].value:untaint()
    if shapers.names[shaper] == "1" then
      shapers.active = shapers.active + 1
      shapers.light = "1"
    end
  end
end

local data = {
  html = format(N("<strong %1$s>%2$d Shaper</strong>","<strong %1$s>%2$d Shapers</strong>",shapers.enabled),shape_link,shapers.enabled).." enabled on "..format(N("<strong %1$s>%2$d interface</strong>","<strong %1$s>%2$d interfaces</strong>",shapers.active),shape_link,shapers.active),
}

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
