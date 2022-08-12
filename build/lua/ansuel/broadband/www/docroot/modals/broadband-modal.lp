--pretranslated: do not change this file

gettext.textdomain('webui-core')

local bridged = require("bridgedmode_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local dyntab_helper = require("web.dyntab_helper")
local post_helper = require("web.post_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local bmh = require("broadbandmode_helper")
local tabdata = dyntab_helper.process(bmh)

local mode_current = tabdata.current.name
local mode_options = tabdata.options
for idx,opt in pairs(mode_options) do
  if opt[1] == "bridge" then
    table.remove(mode_options,idx)
    break
  end
end

local mapParam = {
	wsens = "uci.wansensing.global.enable",
}
local mapValid = {
	wsens = post_helper.validateBoolean,
}

local result = require("web.lte-utils").getContent("rpc.mobiled.DeviceNumberOfEntries")
local devices = tonumber(result.DeviceNumberOfEntries)
if devices and devices > 0 then
  mapParam["wwan_enabled"] = "uci.network.interface.@wwan.enabled"
  mapValid["wwan_enabled"] = post_helper.validateBoolean
  mapParam["wwan_metric"] = "uci.network.interface.@wwan.metric"
  mapValid["wwan_metric"] = function(value,_,_)
    return value == "20" or value == "0"
  end
  mapParam["wwan_profile"] = "uci.network.interface.@wwan.profile"
end

local content = post_helper.handleQuery(mapParam,mapValid)
if devices and devices > 0 and content["wwan_metric"] == "" then
  content["wwan_metric"] = "0"
end

if ngx.var.request_method == "POST" then
  local content1 = ngx.req.get_post_args(0)
  local action = content1["action"]
  if action == "BRIDGED" then
    if not bridged.isBridgedMode() then
      if bridged.configBridgedMode() then
        return bridged.resetreboot("rpc.system.reboot","GUI")
      end
    end
  elseif action == "ROUTED" then
    if bridged.isBridgedMode() then
      if bridged.configRoutedMode() then
        return bridged.resetreboot("rpc.system.reboot","GUI")
      end
    end
  end
end

if not tabdata.current.ajax then
	-- only display if not doing an ajax request
	ngx.print(ui_helper.createHeader(T"Broadband",true,true))

	ngx.print('\
  <div class="modal-body update">\
    '); lp.include("tabs-broadband.lp"); ngx.print('\
    <form class="form-horizontal" method="post" action="/modals/broadband-modal.lp">\
    ',ui_helper.createMessages(message_helper.popMessages()))
  if not bridged.isBridgedMode() then
    ngx.print('\
      <fieldset id="conn_mode" class="advanced hide">\
        <legend>');ngx.print(T"Broadband Type");ngx.print('</legend>')
        local html = {}
        local mode_attributes = {
          group = {
            class = "monitor-wsens monitor-0",
          },
          radio = {
            class = "inline"
          },
          input = {
            class = "no-save",
            ["data-for"] = "SWITCH_MODE",
          }
        }
        local wsensswitchclass = {
          input = {
            class = "monitor-changes save",
          },
        }
        local mode_button = {
          button = {
            ["data-name"] = "action",
            ["data-value"] = "SWITCH_MODE",
            class = "monitor-wsens monitor-0",
          }
        }
        local info_box = {
          alert = {
            class = "alert alert-info",
          }
        }
        local warn_box = {
          alert = {
            class = "alert alert-warn monitor-wsens monitor-0",
          }
        }

        html[#html + 1] = ui_helper.createAlertBlock(T"WAN Sensing sets the connection mode automatically when enabled.",info_box)
        if content["wsens"] == "1" then
          html[#html + 1] = ui_helper.createAlertBlock(T"Disable WAN Sensing to set the connection mode manually.<br/>After saving the change, the connection mode options will be shown.",info_box)
        end
        html[#html+1] = ui_helper.createSwitch(T"WAN Sensing","wsens",content["wsens"],wsensswitchclass)
        if content["wsens"] == "0" then
          html[#html + 1] = ui_helper.createInputRadio(T"Connection Mode","newmode",mode_options,mode_current,mode_attributes)
          html[#html + 1] = ui_helper.createButton("",T"Apply Connection Mode","icon-cog",mode_button)
          html[#html + 1] = ui_helper.createAlertBlock(T"You should apply the Connection Mode after disabling WAN Sensing, even if the required mode is already selected!",warn_box)
          if devices and devices > 0 then
            local pdptype
            local profiles = require("web.content_helper").getMatchedContent("uci.mobiled.profile.")
            for _,profile in ipairs(profiles) do
              if profile.id == content.wwan_profile then
                pdptype = profile.pdptype
                break
              end
            end
            html[#html + 1] = "<fieldset class='monitor-wsens monitor-0 monitor-hide-wsens'><legend>"..T"Permanent WWAN".."</legend>"
            html[#html + 1] = ui_helper.createAlertBlock(T"When enabled, Mobile Internet will be permanently connected at a lower priority so that normal traffic will pass through the WAN connection, but if that fails, traffic will be routed immediately through the WWAN connection.",info_box)
            html[#html + 1] = ui_helper.createAlertBlock(T"Permanent WWAN is <B>NOT</B> recommended with the Telstra Backup SIM. Your SIM will be disabled by Telstra.",warn_box)
            if pdptype and pdptype ~= "ipv4" then
              html[#html + 1] = ui_helper.createAlertBlock(T"Mobile should be configured with an IPv4 <i>only</i> profile, as IPv6 cannot be configured for a lower priority, and normal IPv6 traffic may pass over the WWAN connection, even if the WAN connection is active.",warn_box)
            end
            html[#html + 1] = ui_helper.createSwitch(T"Enabled","wwan_enabled",content["wwan_enabled"])
            html[#html + 1] = ui_helper.createSimpleInputHidden("wwan_metric",content["wwan_metric"])
            html[#html + 1] = [[ <script>$("#wwan_enabled").change(function() { $("input[name=wwan_metric]").val(this.value == "1" ? "20" : "0"); });</script>]]
            html[#html + 1] = "</fieldset>"
          end
        end
      ngx.print(html,'\
    </fieldset>\
    ')
  end
end
lp.include(tabdata.current.view)
if not tabdata.current.ajax then
  -- only display if not doing an ajax request
  ngx.print('\
  <fieldset>\
    <legend>',T"Network Mode",'</legend>\
    ');
    if not bridged.isBridgedMode() then
      local html = {}
      bridged.addBridgedModeButtons(html)
      ngx.print(html)
    else
      local html = {}
      bridged.addRoutedModeButtons(html)
      ngx.print(html)
    end
    ngx.print('\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter(),'\
<script src="/js/bridgedmode.js"></script>\
');
end
