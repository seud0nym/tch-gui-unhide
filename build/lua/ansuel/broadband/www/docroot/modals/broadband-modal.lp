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
local content = post_helper.handleQuery(mapParam,mapValid)

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
    ')
      ngx.print(ui_helper.createMessages(message_helper.popMessages()))
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
        end
      ngx.print(html)
      ngx.print('\
    </fieldset>\
    ')
  end
end
lp.include(tabdata.current.view)
if not tabdata.current.ajax then
    -- only display if not doing an ajax request
    ngx.print('\
    <fieldset>\
      <legend>');  ngx.print( T"Network Mode" ); ngx.print('</legend>\
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
  ')
	ngx.print(ui_helper.createFooter())
  ngx.print('\
  <script src="/js/bridgedmode.js"></script>\
  ');
end
