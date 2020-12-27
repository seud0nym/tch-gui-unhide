--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local format = string.format
local table = table
local tonumber = tonumber

-- Devices data
local devices_data = {
  numWireless = "sys.hosts.ActiveWirelessNumberOfEntries",
  numEthernet = "sys.hosts.ActiveEthernetNumberOfEntries",
}
content_helper.getExactContent(devices_data)

local nAgtDevices = 0
local agents = proxy.get("Device.Services.X_TELSTRA_MultiAP.Controller.MultiAPAgentNumberOfEntries")
if agents and tonumber(agents[1].value) > 0 then
  for i = 1,tonumber(agents[1].value),1 do
    local devices = tonumber(format("%s", proxy.get("Device.Services.X_TELSTRA_MultiAP.Agent." .. i .. ".AssociatedDeviceNumberOfEntries")[1].value) or 0)
    if devices then
      nAgtDevices = nAgtDevices + devices
    end
  end
end

local session = ngx.ctx.session
if session:hasAccess("/modals/device-modal.lp") then
  ngx.print('\
\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeaderNoIcon(T"Devices", "modals/device-modal.lp")  ); ngx.print('\
    <div class="content">\
      ');  
        local nEth = tonumber(devices_data["numEthernet"]) or 0
        local nWiFi = tonumber(devices_data["numWireless"]) or 0
        if nAgtDevices > 0 then
          nEth = nEth - nAgtDevices
          nWiFi = nWiFi + nAgtDevices
        end
        local modal_link='class="modal-link" data-toggle="modal" data-remote="modals/device-modal.lp" data-id="device-modal"'
        ngx.print(
            '<p class="subinfos">',
            format( N('<strong %1$s>%2$d ethernet device</strong> connected',
                      '<strong %1$s>%2$d ethernet devices</strong> connected', nEth),
                      modal_link, nEth),
            '</p>',
            '<p class="subinfos">',
            format( N('<strong %1$s>%2$d Wi-Fi device</strong> connected',
                      '<strong %1$s>%2$d Wi-Fi devices</strong> connected', nWiFi),
                      modal_link, nWiFi),
            '</p>'
        )
        ngx.print('\
    </div>\
  </div>\
</div>\
');  
end
  ngx.print('\
'); 