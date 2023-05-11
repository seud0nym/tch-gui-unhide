--pretranslated: do not change this file

gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local find,sub = string.find,string.sub

local ethPortPath = "uci.ethernet.port."
local ethIntf
local ethIntfList = proxy.getPN(ethPortPath,true)
if ethIntfList ~= nil then
  for _,v in pairs(ethIntfList) do
    if proxy.get(v.path.."wan")[1].value:untaint() == "1" then
      local _,intfStart = find(v.path,ethPortPath.."@")
      if intfStart ~= nil then
        local intfEnd = find(sub(v.path,intfStart+1),"%.")
        if intfEnd ~= nil then
          ethIntf = sub(sub(v.path,intfStart+1),1,intfEnd-1)
          break
        end
      end
    end
  end
end

local wan = {
  iface = "uci.network.interface.@wan.ifname",
}
content_helper.getExactContent(wan)

local iface = string.match(wan.iface,"([^%.]+)")
local deviceIfname = ""
local stats
if iface then
  local deviceDetails = proxy.get(string.format("uci.network.device.@%s.ifname",iface))
  if deviceDetails then
    deviceIfname = deviceDetails[1].value
  end
end
if (iface and find(iface,ethIntf)) or (ethIntf and find(deviceIfname,ethIntf)) then
  ethIntf = ethIntf:untaint()
  stats = {
    operstate = "sys.class.net.@"..ethIntf..".operstate",
    carrier = "sys.class.net.@"..ethIntf..".carrier",
  }
  content_helper.getExactContent(stats)
else
  stats = {
    operstate = "down",
    carrier = "0",
  }
end

local intf_state
local intf_state_map = {
  down = T"Ethernet connection down",
  disconnected = T"Ethernet up but link down",
  connected = T"Ethernet connected",
}

local intf_light_map = {
  down = "red",
  disconnected = "orange",
  connected = "green",
}

if stats.operstate == "up" then
  if stats.carrier ~= "0" then
    intf_state = "connected"
  else
    intf_state = "disconnected"
  end
else
  intf_state = "down"
end

  ngx.print('\
\
<fieldset>\
  <legend>');  ngx.print( T"Physical Link Status" ); ngx.print('</legend>\
\
  ');
  local html = {}
  html[#html + 1] = ui_helper.createLight(T"Status",nil,intf_state_map[intf_state],{ light = { class = intf_light_map[intf_state] } })
  ngx.print(html)
  ngx.print('\
</fieldset>\
');