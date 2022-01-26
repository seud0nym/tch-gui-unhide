--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ngx = ngx
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local message_helper = require("web.uimessage_helper")
local booster_helper = require("booster_helper")
local proxy = require("datamodel")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local match = string.match

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local vB = post_helper.validateBoolean
local vPN = post_helper.validatePositiveNum

local mapParams = {
  agent_enabled = "uci.multiap.agent.enabled",
  agent_dhm = "uci.multiap.agent.default_hysteresis_margin",
  controller_enabled = "uci.multiap.controller.enabled",
  cpc_mri = "uci.multiap.controller_policy_config.metrics_report_interval",
  cpc_amcutd = "uci.multiap.controller_policy_config.ap_metrics_channel_utilization_threshold_dbm",
  cpc_smrhm = "uci.multiap.controller_policy_config.sta_metrics_rssi_hysteresis_margin",
  cpc_smrtd = "uci.multiap.controller_policy_config.sta_metrics_rssi_threshold_dbm",
  cpc_slsts = "uci.multiap.controller_policy_config.sta_link_sta_traffic_stats",
  state5g = "uci.wireless.wifi-device.@radio_5G.state",
}

local mapValid = {
  agent_enabled = vB,
  agent_dhm = vPN,
  controller_enabled = vB,
  cpc_mri = vPN,
  cpc_amcutd = vPN,
  cpc_smrhm = vPN,
  cpc_smrtd = vPN,
  cpc_slsts = vPN,
}

local appath = booster_helper.findBackhaulAPPath()
if appath then
  mapParams["apstate"] = appath.."state"
  mapParams["wl1_2ssid"] = "uci.wireless.wifi-iface.@wl1_2.ssid"
  mapParams["wl1_2state"] = "uci.wireless.wifi-iface.@wl1_2.state"
  mapValid["apstate"] = vB
  mapValid["wl1_2state"] = vB
end

local bandlock = proxy.get("uci.multiap.bandlock.supported")
  if bandlock and bandlock[1].value == "1" then
    bandlock = "1"
    mapParams["bandlock"] = "uci.multiap.bandlock.active"
    mapValid["bandlock"] = vB
  end

local content = post_helper.handleQuery(mapParams,mapValid)

local ssids,synced = booster_helper.getSSIDList()

if ngx.var.request_method == "POST" then
  local args = ngx.req.get_post_args()
  if args["action"] == "SAVE" then
    local gVChkbox = post_helper.getValidateCheckboxSwitch()
    gVChkbox(args["reset_ssids"],args,"reset_ssids")
    if args["reset_ssids"] == "1" then
      local ap = {}
      for _,v in ipairs(proxy.getPN("uci.wireless.wifi-ap.",true)) do
        local path = v.path
        local iface = untaint(proxy.get(path.."iface")[1].value)
        ap[iface] = path
      end
      local do_apply = false
      for _,credential in pairs(ssids) do
        if credential.synced == true then
          local iface = match(credential.wireless_path,".*@(.*)%.")
          ngx.log(ngx.ALERT,"Resetting iface ",iface," to original SSID ",credential.ssid)
          proxy.set(credential.wireless_path.."ssid",credential.ssid)
          proxy.set(ap[iface]..".wpa_psk_key",credential.password)
          do_apply = true
        end
      end
      if do_apply then
        proxy.apply()
      end
    end
  end
end

local switch_class_enable = {
  input = {
    class = "monitor-changes",
  }
}
local agent_enabled_monitor = {
  group = {
    class = "monitor-agent_enabled monitor-1",
  },
  input = {
    type = "number",
    style = "width:80px;"
  }
}
local controller_enabled_monitor = {
  group = {
    class = "monitor-controller_enabled monitor-1",
  },
  input = {
    type = "number",
    style = "width:80px;"
  }
}

ngx.print( ui_helper.createHeader(T"Wi-Fi Boosters",false,true,nil)); ngx.print('\
<div class="modal-body update">\
');  lp.include("tabs-boosters.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/wireless-boosters-modal.lp">\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()));

    local html = {}
    html[#html+1] = "<fieldset><legend>"..T"Multi-AP".."</legend>"
    html[#html+1] = "<legend><small>"..T"Agent".."</small></legend>"
    html[#html+1] = ui_helper.createSwitch(T"Enabled","agent_enabled",content["agent_enabled"],switch_class_enable)
    html[#html+1] = ui_helper.createInputText(T"Default Hysteresis Margin","agent_dhm",content["agent_dhm"],agent_enabled_monitor)
    if synced > 0 then
      local agent_disabled_monitor = {
        group = {
          class = "monitor-agent_enabled monitor-0",
        },
      }
      html[#html+1] = ui_helper.createCheckboxSwitch(T"Reset Default SSIDs","reset_ssids","0",agent_disabled_monitor)
    end
    html[#html+1] = "<legend><small>"..T"Controller".."</small></legend>"
    html[#html+1] = ui_helper.createSwitch(T"Enabled","controller_enabled",content["controller_enabled"],switch_class_enable)
    html[#html+1] = ui_helper.createInputText(T"Metrics Report Interval","cpc_mri",content["cpc_mri"],controller_enabled_monitor)
    html[#html+1] = ui_helper.createInputText(T"AP Metrics Channel Utilization Threshold dBm","cpc_amcutd",content["cpc_amcutd"],controller_enabled_monitor)
    html[#html+1] = ui_helper.createInputText(T"STA Metrics RSSI Hysteresis Margin","cpc_smrhm",content["cpc_smrhm"],controller_enabled_monitor)
    html[#html+1] = ui_helper.createInputText(T"STA Metrics RSSI Threshold dBm","cpc_smrtd",content["cpc_smrtd"],controller_enabled_monitor)
    html[#html+1] = ui_helper.createInputText(T"STA Link STA Traffic Stats","cpc_slsts",content["cpc_slsts"],controller_enabled_monitor)
    if bandlock == "1" then
      html[#html+1] = "<legend><small>"..T"Band Lock".."</small></legend>"
      html[#html+1] = ui_helper.createSwitch(T"Active","bandlock",content["bandlock"])
    end
    html[#html+1] = "</fieldset>"
    if appath then
      html[#html+1] = "<fieldset><legend>"..T"Wireless Backhaul".."</legend>"
      html[#html+1] = ui_helper.createSwitch(T"SSID "..content["wl1_2ssid"],"wl1_2state",content["wl1_2state"])
      html[#html+1] = ui_helper.createSimpleInputHidden("apstate",content["apstate"])
      html[#html+1] = "</fieldset>"
    end
    ngx.print(html)
    ngx.print('\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter())
if appath then
  ngx.print('\
<script>\
$("#wl1_2state").change(function() { $("input[name=apstate]").val(this.value); });\
</script>\
  ');
end