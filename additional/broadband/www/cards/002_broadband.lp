--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local format = string.format
local floor = math.floor
local tonumber = tonumber
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local content_helper = require("web.content_helper")

-- wan status data
local wan_data = {
  wan_ifname        = "uci.network.interface.@wan.ifname",
  wans_enable       = "uci.wansensing.global.enable",
  wans_l2type       = "uci.wansensing.global.l2type",
  dsl_status        = "sys.class.xdsl.@line0.Status",
  dsl_linerate_up   = "sys.class.xdsl.@line0.UpstreamCurrRate",
  dsl_linerate_down = "sys.class.xdsl.@line0.DownstreamCurrRate",
  ethwan_status     = "sys.eth.port.@eth4.status",
  dsl0_enabled      = "uci.xdsl.xdsl.@dsl0.enabled",
}
content_helper.getExactContent(wan_data)
local mobiled_state = {
  mob_session_state = "rpc.mobiled.device.@1.network.sessions.@1.session_state"
}
content_helper.getExactContent(mobiled_state)

local WS_en = wan_data.wans_enable
local WS_mode = wan_data.wans_l2type
if WS_mode == "" then
  WS_mode = "NONE"
end
local WS_State
if WS_en == "1" then
   if WS_mode == "NONE" then
      WS_State = "None"
      WS_state_map = T"WAN Sensing L2: None"
   else
      WS_State = "Enabled"
      WS_state_map = format(T"WAN Sensing L2: %s",WS_mode)
   end
else
   WS_State = "Disabled"
   WS_state_map = T"WAN Sensing: Disabled"
end

local WS_light_map = {
  Disabled = "off",
  None = "orange",
  Enabled = "green",
}

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
content_rpc["uptime"] = content_helper.readfile("/proc/uptime","number",floor)

local function bytes2string(s_bytes)
  if s_bytes=="" then
    return "0<small>B</small>"
  else
    local bytes = tonumber(s_bytes)
    local kb = bytes/1024
    local mb = kb/1024
    local gb = mb/1024
    if gb >= 1 then
      return format("%.2f", gb) .. "<small>GB</small>"
    elseif mb >= 1 then
      return format("%.2f", mb) .. "<small>MB</small>"
    elseif kb >= 1 then
      return format("%.2f", kb) .. "<small>KB</small>"
    else
      return format("%d", s_bytes) .. "<small>B</small>"
    end
  end
end
local session = ngx.ctx.session
local modalPath
if session:hasAccess("/modals/broadband-modal.lp") then
    modalPath = "modals/broadband-modal.lp"
end

  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeader(T"Broadband", modalPath, nil, nil, nil) ); ngx.print('\
    <div class="content card_bg mirror" data-bg-text="&#xf0c1;">\
    ');  ngx.print( ui_helper.createSimpleLight(nil, WS_state_map, { light = { class = WS_light_map[WS_State] } }) ); ngx.print('\
    ');  
      local html = {}
      if wan_data["wan_ifname"] and (wan_data["wan_ifname"] == "ptm0" or wan_data["wan_ifname"] == "atmwan") then
        if wan_data["dsl_status"] == "Up" then
          html[#html+1] = ui_helper.createSimpleLight("1", "DSL connected")
          -- After disabling broadband the page immediately refreshes. At this time the state is still up but the line
          -- rate is already cleared.
          local rate_up = tonumber(wan_data["dsl_linerate_up"])
          local rate_down = tonumber(wan_data["dsl_linerate_down"])
          if rate_up and rate_down then
            rate_up = floor(rate_up / 10) / 100
            rate_down = floor(rate_down / 10) / 100
            html[#html+1] = format('<p class="bbstats"><i class="icon-upload icon-small gray"></i> %.2f<small>Mbps</small> <i class="icon-download icon-small gray"></i> %.2f<small>Mbps</small></p>', rate_up, rate_down)
          end
        elseif wan_data["dsl_status"] == "NoSignal" then
          html[#html+1] = ui_helper.createSimpleLight("4", "DSL disconnected")
        elseif wan_data["dsl0_enabled"] == "0" then
          html[#html+1] = ui_helper.createSimpleLight("0", "DSL disabled")
        else
          html[#html+1] = ui_helper.createSimpleLight("2", "DSL connecting")
        end
      end
      if wan_data["wan_ifname"] and string.find(wan_data["wan_ifname"],"eth") then
        if wan_data["ethwan_status"] == "up" then
          html[#html+1] = ui_helper.createSimpleLight("1", "Ethernet connected")
        else
          html[#html+1] = ui_helper.createSimpleLight("4", "Ethernet disconnected")
        end
      end
      if mobiled_state["mob_session_state"] == "connected" then
        html[#html+1] = ui_helper.createSimpleLight("1", "Mobile connected")
      end
      if tonumber(content_rpc.rx_bytes) and tonumber(content_rpc.tx_bytes) and tonumber(content_rpc.uptime) then
        html[#html+1] = format('<span class="simple-desc"><i class="icon-cloud-upload icon-small status-icon"></i> <span id="broadband-card-upload">%s</span> <i class="icon-cloud-download icon-small status-icon"></i> <span id="broadband-card-download">%s</span> <span id="broadband-card-daily-average">%s</span>/<i>d</i></span>', bytes2string(content_rpc.tx_bytes), bytes2string(content_rpc.rx_bytes), bytes2string((content_rpc.rx_bytes+content_rpc.tx_bytes)/(content_rpc.uptime/86400)))
      end
      ngx.print(html)
      ngx.print('\
    </div>\
  </div>\
</div>\
'); 