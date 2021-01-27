--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-qos')

local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local find, format, match = string.find, string.format, string.match
local session = ngx.ctx.session

local modal_link
local queue_link
local class_link
local shape_link

if session:hasAccess("/modals/qos-queue-modal.lp") and session:hasAccess("/modals/qos-classify-modal.lp") then -- and session:hasAccess("/modals/qos-swshaper-modal.lp") then
  modal_link = "/modals/qos-queue-modal.lp"
  queue_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-queue-modal.lp" data-id="device-modal"'
  class_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-classify-modal.lp" data-id="device-modal"'
  shape_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-swshaper-modal.lp" data-id="device-modal"'
end

local dataQoS = {
  numClassify = "uci.qos.classifyNumberOfEntries",
  numQueues = "uci.qos.classNumberOfEntries",
  numShapers = "uci.qos.swshaperNumberOfEntries",
}
content_helper.getExactContent(dataQoS)

local classify = tonumber(dataQoS.numClassify)
if not classify then
  classify = 0
end
local queues = tonumber(dataQoS.numQueues)
if not queues then
  queues = 0
end
local shapers = {
  count = tonumber(dataQoS.numShapers),
  active = 0,
  light = "0",
  names = {},
}
if not shapers.count then
  shapers.count = 0
else
  local swshapers = proxy.getPN("uci.qos.swshaper.",true)
  for _,v in ipairs(swshapers) do
    local name = match(v.path, "uci%.qos%.swshaper%.@([^%.]+)%.")
    shapers.names[name] = proxy.get(v.path .. "enable")[1].value:untaint()
  end
end

local inUseDevices = ""
local network = proxy.getPN("uci.network.interface.", true)
for _,v in ipairs(network) do
  local ifname = proxy.get(v.path .. "ifname")[1].value:untaint()
  if ifname and ifname ~= "" and ifname ~= "lo" then
    inUseDevices = inUseDevices .. " " .. ifname
  end
end
local wifi = proxy.getPN("uci.wireless.wifi-iface.", true)
for _,v in ipairs(wifi) do
  local ifname = match(v.path, "uci%.wireless%.wifi%-iface%.@([^%.]+)%.")
  inUseDevices = inUseDevices .. " " .. ifname
end

local group = {}
group["TO_LAN"] = 0
group["TO_WAN"] = 0
group["TO_WLAN"] = 0
local light = {}
light["TO_LAN"] = "0"
light["TO_WAN"] = "0"
light["TO_WLAN"] = "0"

local data = proxy.getPN("uci.qos.device.", true)
for _,v in ipairs(data) do
  local device = match(v.path, "uci%.qos%.device%.@([^%.]+)%.")
  if device == "atm_8_35" then
    device = "atm"
  elseif device == "ptm0" then
    device = "ptm"
  end
  if find(inUseDevices, device) then
    local enabled = proxy.get(v.path .. "enable")[1].value:untaint()
    local classgroup = proxy.get(v.path .. "classgroup")[1].value:untaint()
    local shaper = proxy.get(v.path .. "swshaper")[1].value:untaint()
    if shapers.names[shaper] == "1" then
      shapers.active = shapers.active + 1
      shapers.light = "1"
    end
    if enabled ~= "0" then
      group[classgroup] = group[classgroup] + 1
      light[classgroup] = "1"
    end
  end
end

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
  '); ngx.print( ui_helper.createCardHeader(T"QoS", modal_link) ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf072;">\
      <div class="divtable">\
      ');
      ngx.print(
        ui_helper.createSimpleLight(light.TO_WAN, "QoS enabled on " .. format(N("<strong %1$s>%2$d WAN device</strong>","<strong %1$s>%2$d WAN devices</strong>",group.TO_WAN),queue_link,group.TO_WAN)),
        ui_helper.createSimpleLight(light.TO_LAN, "QoS enabled on " .. format(N("<strong %1$s>%2$d LAN port</strong>","<strong %1$s>%2$d LAN ports</strong>",group.TO_LAN),queue_link,group.TO_LAN)),
        ui_helper.createSimpleLight(light.TO_WLAN, "QoS enabled on " .. format(N("<strong %1$s>%2$d Wi-Fi SSID</strong>","<strong %1$s>%2$d Wi-Fi SSIDs</strong>",group.TO_WLAN),queue_link,group.TO_WLAN)),
        ui_helper.createSimpleLight(shapers.light, format(N("<strong %1$s>%2$d Upload Shaper</strong> active","<strong %1$s>%2$d Upload Shapers</strong> active",shapers.active),shape_link,shapers.active)),
        '<p class="subinfos">',
        format(N("<strong %1$s>%2$d queue</strong> defined","<strong %1$s>%2$d queues</strong> defined",queues),queue_link,queues),
        '<br>',
        format(N("<strong %1$s>%2$d classify rule</strong> defined","<strong %1$s>%2$d classify rules</strong> defined",classify),class_link,classify),
        '</p>'
      )
      ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
');
