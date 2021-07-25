--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local lp = require("web.lp")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local dyntab_helper = require("web.dyntab_helper")
local imh = require("internetmode_helper")
local tabdata = dyntab_helper.process(imh)

local mode_current = tabdata.current.name
local mode_options = tabdata.options
if mode_current == "" then
  for _,v in ipairs(imh) do
    if v.default == true then
      mode_current = v.name
      break
    end
  end
end

if not tabdata.current.ajax then
  local proxy = require("datamodel")
  local wan_ppp_ll_intf = proxy.get("rpc.network.interface.@wan.ppp.ll_intf")
  local wan_ppp_over_atm
  if wan_ppp_ll_intf and wan_ppp_ll_intf[1].value then
    wan_ppp_ll_intf = wan_ppp_ll_intf[1].value
    wan_ppp_over_atm = proxy.get(string.format("uci.xtm.atmdevice.@%s.", wan_ppp_ll_intf)) ~= nil
  else
    wan_ppp_ll_intf = ""
  end
  if not wan_ppp_over_atm then
    for k,v in ipairs(mode_options) do
      if v[1] == "pppoa" then
        table.remove(mode_options, k)
      end
      if mode_current ~= "bridge" and v[1] == "bridge" then
        table.remove(mode_options, k)
      end
    end
  end

  ngx.print(ui_helper.createHeader(T"Internet Access", true, true)); ngx.print('\
  <div class="modal-body update">\
    <form class="form-horizontal" method="post" action="modals/internet-modal.lp">\
    '); ngx.print(ui_helper.createMessages(message_helper.popMessages()))

    local mode_attributes = {
      radio = {
        class = "inline"
      },
      input = {
        class = "no-save",
        ["data-for"] = "SWITCH_MODE",
      }
    }
    local mode_button = {
      button = {
        ["data-name"] = "action",
        ["data-value"] = "SWITCH_MODE",
      }
    }

    local html = {}
    html[#html+1] = '<fieldset id="conn_mode" class="advanced hide">'
    html[#html+1] = '<legend>' .. T"Connection Mode" .. '</legend>'
    html[#html+1] = ui_helper.createInputRadio(T"Mode", "newmode", mode_options, mode_current, mode_attributes)
    html[#html+1] = ui_helper.createButton("", T"Switch Connection Mode", "icon-cog", mode_button)
    html[#html+1] = '</fieldset>'
    ngx.print(html)
end

lp.setpath("/www/snippets/")
lp.include(tabdata.current.view)

if not tabdata.current.ajax then
    ngx.print('\
    </form>\
  </div>\
  '); ngx.print(ui_helper.createFooter());
end
