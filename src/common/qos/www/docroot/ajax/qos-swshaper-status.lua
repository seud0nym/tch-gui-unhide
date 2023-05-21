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

local data = {
  html = format(N("<strong %1$s>%2$d Shaper</strong> enabled","<strong %1$s>%2$d Shapers</strong> enabled",shapers.enabled),shape_link,shapers.enabled),
}

local swshapers = proxy.getPN("Device.QoS.Shaper.",true)
if swshapers then
  local count = 0
  for _,v in ipairs(swshapers) do
    local enabled = proxy.get(v.path.."Enable")
    if enabled and enabled[1].value == "true" then
      count = count + 1
    end
  end

  data.html = data.html.."<br>"..format(N("<strong %1$s>%2$d System Shaper</strong>","<strong %1$s>%2$d System Shapers</strong> enabled",count),shape_link,count)
end

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
