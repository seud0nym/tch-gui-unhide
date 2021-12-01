--pretranslated: do not change this file

gettext.textdomain('webui-core')

local bridged = require("bridgedmode_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local dyntab_helper = require("web.dyntab_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")

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

local function resetreboot(path,value)
  proxy.set(path,value)
  ngx.header.content_type = "application/json"
  ngx.print('{ "success":"true" }')
  ngx.timer.at(0,function()
    proxy.apply()
  end)
  ngx.exit(ngx.HTTP_OK)
end

if ngx.var.request_method == "POST" then
  local content1 = ngx.req.get_post_args()
  local action = content1["action"]
  if action == "BRIDGED" then
    if not bridged.isBridgedMode() then
      bridged.configBridgedMode()
      return resetreboot("rpc.system.reboot","GUI")
    end
  elseif action == "ROUTED" then
    if bridged.isBridgedMode() then
      bridged.configRoutedMode()
      return resetreboot("rpc.system.reboot","GUI")
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

        html[#html + 1] = ui_helper.createAlertBlock(T"WAN Sensing sets the connection mode automatically when enabled.",info_box)
        if content["wsens"] == "1" then
          html[#html + 1] = ui_helper.createAlertBlock(T"Disable WAN Sensing to set the connection mode manually.<br/>After saving the change,the connection mode options will be shown.",info_box)
        end
        html[#html+1] = ui_helper.createSwitch(T"WAN Sensing","wsens",content["wsens"],wsensswitchclass)
        if content["wsens"] == "0" then
          html[#html + 1] = ui_helper.createInputRadio(T"Connection Mode","newmode",mode_options,mode_current,mode_attributes)
          html[#html + 1] = ui_helper.createButton("",T"Apply Connection Mode","icon-cog",mode_button)
          html[#html + 1] = ui_helper.createAlertBlock(T"You should apply the Connection Mode after disabling WAN Sensing,even if the required mode is already selected!",warn_box)
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
        local bridged_rebooting = {
          alert = {
            class = "alert hide",
            id = "bridged-rebooting-msg"
          }
        }
        local bridged_confirming = {
          alert = {
            class = "alert hide",
            id = "bridged-confirming-msg"
          }
        }
        local bridged_button = {
          button = {
            id = "btn-bridged"
          }
        }
        html[#html + 1] = ui_helper.createButton("Change Mode","Bridged","icon-cog",bridged_button)
        html[#html + 1] = '<div class="control-group controls">'
        html[#html + 1] = ui_helper.createAlertBlock(T"Switching to <strong>Bridged Mode</strong> and restarting,please wait...",bridged_rebooting)
        html[#html + 1] = ui_helper.createAlertBlock(T"Are you sure you want to switch to <strong>Bridged Mode</strong>?",bridged_confirming)
        html[#html + 1] = string.format([[
          <div id="bridged-changes" class="hide">
            <div id="bridged-confirm" class="btn btn-primary" data-dismiss="modal">%s</div>
            <div id="bridged-cancel" class="btn">%s</div>
          </div>
        </div>
        ]],T"Confirm",T"Cancel")
        ngx.print(html)
      else
        local html = {}
        local routed_rebooting = {
          alert = {
            class = "alert hide",
            id = "routed-rebooting-msg"
          }
        }
        local routed_confirming = {
          alert = {
            class = "alert hide",
            id = "routed-confirming-msg"
          }
        }
        local routed_button = {
          button = {
            id = "btn-routed"
          }
        }
        html[#html + 1] = ui_helper.createButton("Change Mode","Routed","icon-cog",routed_button)
        html[#html + 1] = '<div class="control-group controls">'
        html[#html + 1] = ui_helper.createAlertBlock(T"Switching to <strong>Routed Mode</strong> and restarting,please wait...",routed_rebooting)
        html[#html + 1] = ui_helper.createAlertBlock(T"Are you sure you want to switch back to <strong>Routed Mode</strong>?",routed_confirming)
        html[#html + 1] = string.format([[
          <div id="routed-changes" class="hide">
            <div id="routed-confirm" class="btn btn-primary" data-dismiss="modal">%s</div>
            <div id="routed-cancel" class="btn">%s</div>
          </div>
        </div>
        ]],T"Confirm",T"Cancel")
        ngx.print(html)
      end
      ngx.print('\
      </fieldset>\
    </form>\
  </div>\
  ')
	ngx.print(ui_helper.createFooter())
  ngx.print('\
  <script>\
  (function() {\
    var target = $(".modal form").attr("action");\
    function resetreboot(msg,msg_dst,action) {\
      msg_dst.after(msg);\
      msg.removeClass("hide");\
      msg[0].scrollIntoView();\
      $.post(\
        target,\
        { action: action,CSRFtoken: $("meta[name=CSRFtoken]").attr("content") },\
        wait_for_webserver_down,\
        "json"\
      );\
      return false;\
    }\
    $("#btn-bridged").click(function() {\
      $("#bridged-confirming-msg").removeClass("hide");\
      $("#bridged-changes").removeClass("hide");\
      $(".modal-body").animate({\'scrollTop\':"+=100px"},"fast")\
    });\
    $("#bridged-confirm").click(function() {\
      $("#bridged-confirming-msg").addClass("hide");\
      $("#bridged-changes").addClass("hide");\
      $("#btn-bridged").addClass("hide");\
      return resetreboot($("#bridged-rebooting-msg"),$("#btn-bridged"),"BRIDGED");\
    });\
    $("#bridged-cancel").click(function() {\
      $("#bridged-confirming-msg").addClass("hide");\
      $("#bridged-changes").addClass("hide");\
      $("#bridged-rebooting-msg").addClass("hide");\
    });\
    $("#btn-routed").click(function() {\
      $("#routed-confirming-msg").removeClass("hide");\
      $("#routed-changes").removeClass("hide");\
      $(".modal-body").animate({\'scrollTop\':"+=100px"},"fast")\
    });\
    $("#routed-confirm").click(function() {\
      $("#routed-confirming-msg").addClass("hide");\
      $("#routed-changes").addClass("hide");\
      $("#btn-routed").addClass("hide");\
      return resetreboot($("#routed-rebooting-msg"),$("#btn-routed"),"ROUTED");\
    });\
    $("#routed-cancel").click(function() {\
      $("#routed-confirming-msg").addClass("hide");\
      $("#routed-changes").addClass("hide");\
      $("#routed-rebooting-msg").addClass("hide");\
    });\
  }());\
  </script>\
  '); 
end
