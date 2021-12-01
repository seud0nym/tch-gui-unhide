--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-qos')

local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local find,gmatch,format,match = string.find,string.gmatch,string.format,string.match
local session = ngx.ctx.session
local qosdevice_helper = require("qosdevice_helper")

local modal_link = ""
local devic_link = ""
local queue_link = ""
local shape_link = "" -- NOT 17.2
local mqos_link  = ""

if session:hasAccess("/modals/qos-queue-modal.lp") and session:hasAccess("/modals/qos-classify-modal.lp") then
  modal_link = "/modals/qos-swshaper-modal.lp"
  devic_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-device-modal.lp" data-id="qos-modal"'
  queue_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-queue-modal.lp" data-id="qos-modal"'
  shape_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-swshaper-modal.lp" data-id="qos-modal"' -- NOT 17.2
  mqos_link  = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-mqos-modal.lp" data-id="qos-modal"'
end

local function queueCount(classes)
  local count = 0
  if classes then
    for _ in gmatch(untaint(classes),"[^ ]+") do
      count = count + 1
    end
  end
  return count
end

local dataQoS = {
  mqos_enabled = "uci.mqos.global.enabled",
  mqos_devices = "uci.mqos.deviceNumberOfEntries",
  numShapers = "uci.qos.swshaperNumberOfEntries",-- NOT 17.2
  wan_queues = "uci.qos.classgroup.@TO_WAN.classes",
  lan_queues = "uci.qos.classgroup.@TO_LAN.classes",
	wlan_queues = "uci.qos.classgroup.@TO_WLAN.classes",
}
content_helper.getExactContent(dataQoS)

local mqos_status
if dataQoS.mqos_enabled == "1" then
  mqos_status = "enabled"
else
  mqos_status = "disabled"
end

local lan_queues = queueCount(dataQoS.lan_queues)
local wan_queues = queueCount(dataQoS.wan_queues)
local wlan_queues = queueCount(dataQoS.wlan_queues)

-- NOT 17.2 START
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
-- NOT 17.2 END

local group = {}
group["TO_LAN"] = 0
group["TO_WAN"] = 0
group["TO_WLAN"] = 0
local light = {}
light["TO_LAN"] = "0"
light["TO_WAN"] = "0"
light["TO_WLAN"] = "0"

local ifs = qosdevice_helper.getNetworkDevices()

local data = proxy.getPN("uci.qos.device.",true)
for _,v in ipairs(data) do
  local device = untaint(match(v.path,"uci%.qos%.device%.@([^%.]+)%."))
  if ifs[device] and ifs[device] ~= "" and ifs[device] ~= "ppp" and ifs[device] ~= "ipoe" then
    local enabled = proxy.get(v.path.."enable")[1].value:untaint()
    if enabled ~= "0" then
      local classgroup = proxy.get(v.path.."classgroup")[1].value:untaint()
      group[classgroup] = group[classgroup] + 1
      if light[classgroup] ~= "2" then
        if (classgroup == "TO_WAN" and not find(ifs[device],"wan")) or (classgroup == "TO_WLAN" and not find(ifs[device],"wlan")) or (classgroup == "TO_LAN" and not match(ifs[device],"^lan") and not find(ifs[device],",lan")) then
          light[classgroup] = "2"
        else
          light[classgroup] = "1"
        end
      end    
    end
    -- NOT 17.2 START
    local shaper = proxy.get(v.path.."swshaper")[1].value:untaint()
    if shapers.names[shaper] == "1" then
      shapers.active = shapers.active + 1
      shapers.light = "1"
    end
    -- NOT 17.2 END
  end
end

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
  '); ngx.print( ui_helper.createCardHeader(T"QoS",modal_link) ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf072;">\
      <div class="divtable">\
      ');
      ngx.print(
        ui_helper.createSimpleLight(shapers.light,format(N("<strong %1$s>%2$d Shaper</strong>","<strong %1$s>%2$d Shapers</strong>",shapers.enabled),shape_link,shapers.enabled).." enabled on "..format(N("<strong %1$s>%2$d interface</strong>","<strong %1$s>%2$d interfaces</strong>",shapers.active),shape_link,shapers.active)), -- NOT 17.2
        ui_helper.createSimpleLight(dataQoS.mqos_enabled,format("<strong %1$s>MAC Address Shaping</strong> %2$s",mqos_link,mqos_status))
      )
      if dataQoS.mqos_enabled == "1" then
        ngx.print(
          '<p class="subinfos">',
          format(N("<strong %1$s>%2$d MAC Address</strong> shaped","<strong %1$s>%2$d MAC Addresses</strong> shaped",dataQoS.mqos_devices),mqos_link,dataQoS.mqos_devices),
          '</p>'
        )
      end
      ngx.print(
        ui_helper.createSimpleLight(light.TO_WAN,format(N("<strong %1$s>%2$d Queue</strong>","<strong %1$s>%2$d Queues</strong>",wan_queues),queue_link,wan_queues).." on "..format(N("<strong %1$s>%2$d WAN interface</strong>","<strong %1$s>%2$d WAN interface</strong>",group.TO_WAN),devic_link,group.TO_WAN)),
        ui_helper.createSimpleLight(light.TO_LAN,format(N("<strong %1$s>%2$d Queue</strong>","<strong %1$s>%2$d Queues</strong>",lan_queues),queue_link,lan_queues).." on "..format(N("<strong %1$s>%2$d LAN port</strong>","<strong %1$s>%2$d LAN ports</strong>",group.TO_LAN),devic_link,group.TO_LAN)),
        ui_helper.createSimpleLight(light.TO_WLAN,format(N("<strong %1$s>%2$d Queue</strong>","<strong %1$s>%2$d Queues</strong>",wlan_queues),queue_link,wlan_queues).." on "..format(N("<strong %1$s>%2$d Wi-Fi SSID</strong>","<strong %1$s>%2$d Wi-Fi SSIDs</strong>",group.TO_WLAN),devic_link,group.TO_WLAN))
      )
      ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
');
