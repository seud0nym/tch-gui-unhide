--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-qos')

local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local find, gfind, format, match, untaint = string.find, string.gfind, string.format, string.match, string.untaint
local session = ngx.ctx.session
local qosdevice_helper = require("qosdevice_helper")

local modal_link
local queue_link
local class_link
local recla_link
local shape_link

if session:hasAccess("/modals/qos-queue-modal.lp") and session:hasAccess("/modals/qos-classify-modal.lp") then
  modal_link = "/modals/qos-device-modal.lp"
  queue_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-queue-modal.lp" data-id="qos-modal"'
  class_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-classify-modal.lp" data-id="qos-modal"'
  recla_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-reclassify-modal.lp" data-id="qos-modal"'
  shape_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-swshaper-modal.lp" data-id="qos-modal"'
end

local function queueCount(classes)
  local count = 0
  if classes then
    for q in gfind(untaint(classes), "[^ ]+") do
      count = count + 1
    end
  end
  return count
end

local dataQoS = {
  numClassify = "uci.qos.classifyNumberOfEntries",
  numReclassify = "uci.qos.reclassifyNumberOfEntries",
  numShapers = "uci.qos.swshaperNumberOfEntries",
  wan_queues = "uci.qos.classgroup.@TO_WAN.classes",
  lan_queues = "uci.qos.classgroup.@TO_LAN.classes",
	wlan_queues = "uci.qos.classgroup.@TO_WLAN.classes",
}
content_helper.getExactContent(dataQoS)

local classify = tonumber(dataQoS.numClassify)
if not classify then
  classify = 0
end
local reclassify = tonumber(dataQoS.numReclassify)
if not reclassify then
  reclassify = 0
end

local lan_queues = queueCount(dataQoS.lan_queues)
local wan_queues = queueCount(dataQoS.wan_queues)
local wlan_queues = queueCount(dataQoS.wlan_queues)

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

local group = {}
group["TO_LAN"] = 0
group["TO_WAN"] = 0
group["TO_WLAN"] = 0
local light = {}
light["TO_LAN"] = "0"
light["TO_WAN"] = "0"
light["TO_WLAN"] = "0"

local ifs = qosdevice_helper.getNetworkDevices()

local data = proxy.getPN("uci.qos.device.", true)
for _,v in ipairs(data) do
  local device = untaint(match(v.path, "uci%.qos%.device%.@([^%.]+)%."))
  if device == "atm_8_35" then
    device = "atm"
  elseif device == "ptm0" then
    device = "ptm"
  end
  if ifs[device] then
    local enabled = proxy.get(v.path .. "enable")[1].value:untaint()
    local classgroup = proxy.get(v.path .. "classgroup")[1].value:untaint()
    local shaper = proxy.get(v.path .. "swshaper")[1].value:untaint()
    if shapers.names[shaper] == "1" then
      shapers.active = shapers.active + 1
      shapers.light = "1"
    end
    if enabled ~= "0" then
      group[classgroup] = group[classgroup] + 1
      if light[classgroup] ~= "2" then
        if (classgroup == "TO_WAN" and not find(ifs[device], "wan")) or (classgroup == "TO_WLAN" and not find(ifs[device], "wlan")) or (classgroup == "TO_LAN" and not match(ifs[device], "^lan") and not find(ifs[device], ",lan")) then
          light[classgroup] = "2"
        else
          light[classgroup] = "1"
        end
      end    
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
        ui_helper.createSimpleLight(light.TO_WAN, format(N("%d queue","%d queues",wan_queues),wan_queues).." on "..format(N("<strong %1$s>%2$d WAN device</strong>","<strong %1$s>%2$d WAN devices</strong>",group.TO_WAN),queue_link,group.TO_WAN)),
        ui_helper.createSimpleLight(light.TO_LAN, format(N("%d queue","%d queues",lan_queues),lan_queues).." on "..format(N("<strong %1$s>%2$d LAN port</strong>","<strong %1$s>%2$d LAN ports</strong>",group.TO_LAN),queue_link,group.TO_LAN)),
        ui_helper.createSimpleLight(light.TO_WLAN, format(N("%d queue","%d queues",wlan_queues),wlan_queues).." on "..format(N("<strong %1$s>%2$d Wi-Fi SSID</strong>","<strong %1$s>%2$d Wi-Fi SSIDs</strong>",group.TO_WLAN),queue_link,group.TO_WLAN)),
        ui_helper.createSimpleLight(shapers.light, format(N("<strong %1$s>%2$d Upload Shaper</strong> active","<strong %1$s>%2$d Upload Shapers</strong> active",shapers.active),shape_link,shapers.active)),
        '<p class="subinfos">',
        format(N("<strong %1$s>%2$d classify rule</strong> defined","<strong %1$s>%2$d classify rules</strong> defined",classify),class_link,classify),
        '<br>',
        format(N("<strong %1$s>%2$d reclassify rule</strong> defined","<strong %1$s>%2$d reclassify rules</strong> defined",reclassify),recla_link,reclassify),
        '</p>'
      )
      ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
');
