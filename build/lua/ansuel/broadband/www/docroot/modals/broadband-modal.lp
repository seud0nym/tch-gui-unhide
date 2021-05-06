--pretranslated: do not change this file

gettext.textdomain('webui-core')

local bridged = require("bridgedmode_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local dyntab_helper = require("web.dyntab_helper")
local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local bmh = require("broadbandmode_helper")
local tabdata = dyntab_helper.process(bmh)

local mode_current = tabdata.current.name
local mode_options = tabdata.options
local bridge_mode_options = {}
local idx, opt 
for idx,opt in pairs(mode_options) do
  if opt[1] == "bridge" then
    bridge_mode_options[#bridge_mode_options+1] = opt
    table.remove(mode_options,idx)
    break
  end
end    

local mapParam = {
	wsens = "uci.wansensing.global.enable"
}
local mapValid = {
	wsens = post_helper.validateBoolean,
}

content, helpmsg = post_helper.handleQuery(mapParam, mapValid)

if not tabdata.current.ajax then
	-- only display if not doing an ajax request
	ngx.print(ui_helper.createHeader(T"Broadband", true, true))

	ngx.print('\
  <div class="modal-body update">\
    '); lp.include("tabs-broadband.lp") ngx.print('\
    <form class="form-horizontal" method="post" action="/modals/broadband-modal.lp">\
    ')
      ngx.print(ui_helper.createMessages(message_helper.popMessages()))
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
        if bridged.isBridgedMode() then
          html[#html + 1] = ui_helper.createInputRadio(T"Connection Mode", "newmode", bridge_mode_options, mode_current, mode_attributes)
        else
          local wsensswitchclass = {
            input = {
              class = "monitor-changes",
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

          html[#html + 1] = ui_helper.createAlertBlock(T"WAN Sensing sets the connection mode automatically when enabled.", info_box)
          if content["wsens"] == "1" then
            html[#html + 1] = ui_helper.createAlertBlock(T"Disable WAN Sensing to set the connection mode manually.<br/>After saving the change, the connection mode options will be shown.", info_box)
          end
          html[#html+1] = ui_helper.createSwitch(T"WAN Sensing", "wsens", content["wsens"], wsensswitchclass)
          if content["wsens"] == "0" then
            html[#html + 1] = ui_helper.createInputRadio(T"Connection Mode", "newmode", mode_options, mode_current, mode_attributes)
            html[#html + 1] = ui_helper.createButton("", T"Apply Connection Mode", "icon-cog", mode_button)
            html[#html + 1] = ui_helper.createAlertBlock(T"You should apply the Connection Mode after disabling WAN Sensing, even if the required mode is already selected!", warn_box)
          end
        end
        ngx.print(html)
        ngx.print('\
      </fieldset>\
      ')
end
lp.include(tabdata.current.view)
if not tabdata.current.ajax then
    -- only display if not doing an ajax request
    ngx.print('\
    </form>\
  </div>\
  ')
	ngx.print(ui_helper.createFooter())
end
