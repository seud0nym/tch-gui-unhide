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

local mapParams = {
  agent_enabled = "uci.multiap.agent.enabled",
  controller_enabled = "uci.multiap.controller.enabled",
  state5g = "uci.wireless.wifi-device.@radio_5G.state",
}

local mapValid = {
  agent_enabled = vB,
  controller_enabled = vB,
}


local appath = booster_helper.findBackhaulAPPath()
if appath then
  mapParams["apstate"] = appath.."state"
  mapParams["wl1_2ssid"] = "uci.wireless.wifi-iface.@wl1_2.ssid"
  mapParams["wl1_2state"] = "uci.wireless.wifi-iface.@wl1_2.state"

  mapValid["apstate"] = vB
  mapValid["wl1_2state"] = vB
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

ngx.print('\
\
');  ngx.print( ui_helper.createHeader(T"Wi-Fi Boosters",false,true,nil)); ngx.print('\
<div class="modal-body update">\
');  lp.include("tabs-boosters.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/wireless-boosters-modal.lp">\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()));
    local switch_class_enable = {
      input = {
        class = "monitor-changes",
      }
    }
    local switch_class_monitor = {
      group = {
        class = "monitor-agent_enabled monitor-0 monitor-hidden-agent_enabled",
      },
    }

    local html = {}
    html[#html+1] = "<fieldset><legend>"..T"Multi-AP Status".."</legend>"
    html[#html+1] = ui_helper.createSwitch(T"Controller","controller_enabled",content["controller_enabled"])
    html[#html+1] = ui_helper.createSwitch(T"Agent","agent_enabled",content["agent_enabled"],switch_class_enable)
    if synced > 0 then
      html[#html+1] = ui_helper.createCheckboxSwitch(T"Reset Default SSIDs","reset_ssids","0",switch_class_monitor)
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