--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-voice')

local proxy = require ("datamodel")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local content_helper = require ("web.content_helper")
local message_helper = require("web.uimessage_helper")
local format = string.format

local plan_name = "generic"
local inTableAction = false
if ngx.req.get_method() == "POST" then
  local post_args = ngx.req.get_post_args()
  local selected = format("%s", post_args["dial_plan"] or "")
  if selected then
    plan_name = selected
  end
  local action = post_args["action"]
  if action then
    if action:match("(TABLE-)%w*") then
      inTableAction = true
      plan_name = post_args["tableid"]:match("(%w+)-dialplanentries")
    elseif action == "SAVE" then
      for key, val in pairs(post_args) do
        local prefix, param = key:match("(%w+)-(.+)")
        if prefix == selected then
          local path = "uci.mmpbx.dial_plan.@dial_plan_"..prefix.."."..param
          proxy.set(path, val)
        end
      end
      proxy.apply()
    end    
  end  
end

local plans_base_path = "uci.mmpbx.dial_plan."
local plans_content = proxy.get(plans_base_path)
local plans = {}
local plan_details = {}
if plans_content then
  for _,v in ipairs(plans_content) do
    local path, network = v.path:match("(uci.mmpbx.dial_plan.@dial_plan_%w+%.)(%w*)")
    local name = path:match("uci.mmpbx.dial_plan.@dial_plan_(%w+)%.")
    if #plans == 0 or name ~= plans[#plans][1] then
      if not inTableAction or (inTableAction and name == plan_name) then
        plans[#plans + 1] = { name, T(name:upper()) } 
      end
      plan_details[name] = {}
    end
    if network and network ~= "" then
      if plan_details[name][network] then
        plan_details[name][network] = format("%s, %s", plan_details[name][network], v.value)
      else
        plan_details[name][network] = format("%s", v.value)
      end
    else
      plan_details[name][v.param] = format("%s", v.value)
    end
  end
end


local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
    uri = uri .. "?" .. ngx.var.args
end

local entry_columns = {
  {
    header = T"Enabled",
    name = "enabled",
    param = "enabled",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "left" }}
  },
  {
    header = T"Pattern",
    name = "pattern",
    param = "pattern",
    type = "text",
    attr = { input = { class="span2" } },
  },
  {
    header = T"Min<br>Length",
    name = "min_length",
    param = "min_length",
    type = "text",
    attr = { input = { type = "number", min = "1", class="span2", style = "width:35px" } },
  },
  {
    header = T"Max<br>Length",
    name = "max_length",
    param = "max_length",
    type = "text",
    attr = { input = { type = "number", min = "1", class="span2", style = "width:35px" } },
  },
  {
    header = T"Include<br>EoN",
    name = "include_eon",
    param = "include_eon",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "center" }}
  },
  {
    header = T"Remove<br>Number of<br>Characters",
    name = "remove_number_of_characters",
    param = "remove_number_of_characters",
    type = "text",
    attr = { input = { type = "number", min = "0", class="span2", style = "width:35px" } },
  },
  {
    header = T"Insert<br>Characters",
    name = "insert",
    param = "insert",
    type = "text",
    attr = { input = { type = "number", class="span2", style = "width:60px" } },
  },
  {
    header = T"Postion<br>of<br>Modify",
    name = "position_of_modify",
    param = "position_of_modify",
    type = "text",
    attr = { input = { type = "number", min = "0", class="span2", style = "width:35px" } },
  },
  {
    header = T"Apply<br>Forced<br>Profile",
    name = "apply_forced_profile",
    param = "apply_forced_profile",
    type = "switch",
    attr = { switch = { ["data-placement"] = "center" }}
  },
  {
    header = T"Open<br>Inter-digit<br>Timer",
    name = "open_inter_digit_timer",
    param = "open_inter_digit_timer",
    type = "text",
    attr = { input = { type = "number", min = "0", class="span2", style = "width:60px" } },
  },
  {
    header = T"Priority",
    name = "priority",
    param = "priority",
    type = "text",
    attr = { input = { class="span2" } },
  },
  {
    header = T"Allow",
    name = "allow",
    param = "allow",
    type = "switch",
    attr = { switch = { ["data-placement"] = "center" }}
  },
}

local gOV = post_helper.getOptionalValidation
local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString
local vPN = post_helper.validatePositiveNum

local entry_valid = {
  enabled = vB,
  pattern = vNES,
  min_length = vPN,
  max_length = vPN,
  include_eon = vB,
  remove_number_of_characters = vPN,
  insert = gOV(vPN),
  position_of_modify = vPN,
  apply_forced_profile = vB,
  open_inter_digit_timer = gOV(vPN),
  priority = vNES,
  allow = vB
}

ngx.print(ui_helper.createHeader(T"Telephony", false, true, nil, help_link)); ngx.print('\
<div class="modal-body update">\
');
local lp = require("web.lp")
lp.setpath("/www/snippets/")
lp.include("tabs-voice.lp")
ngx.print('\
  <form class="form-horizontal" method="post" action="'); ngx.print(uri); ngx.print('">\
  '); ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
    <fieldset>\
      <legend>'); ngx.print(T"Dial Plans"); ngx.print('</legend>\
        ')
        local plan_attributes = {
          select = {
            class ="monitor-changes no-save",
          },
        }
        ngx.print(ui_helper.createInputSelect(T"Dial Plan Name", "dial_plan", plans, plan_name, plan_attributes))
        ngx.print('\
    </fieldset>\
    <fieldset id="dial_plan_config">\
      <legend>'); ngx.print(T"Dial Plan Configuration"); ngx.print('</legend>\
        ')
        local no_match_actions = {
          { "allow", T"Allow" },
        }
        local selection_modes = {
          { "legacy", T"Legacy" },
        }
        for _,p in ipairs(plans) do
          local plan = p[1]
          local eon_attributes = {
            group = {
              class ="monitor-dial_plan monitor-"..plan.." monitor-1",
            },
            input = {
              maxlength = "1",
              style = "width:10px",
            }
          }
          local input_attributes = {
            group = {
              class ="monitor-dial_plan monitor-"..plan.." monitor-1",
            },
          }
          local select_attributes = {
            group = {
              class ="monitor-dial_plan monitor-"..plan.." monitor-1",
            },
            select = {
              style = "width:100px",
            }
          }
          local switch_attributes = {
            group = {
              class ="monitor-dial_plan monitor-"..plan.." monitor-1",
            },
          }
          local timer_attributes = {
            group = {
              class ="monitor-dial_plan monitor-"..plan.." monitor-1",
            },
            input = {
              type = "number",
              style = "width:60px",
            }
          }
          local details = plan_details[plan]
          ngx.print(ui_helper.createInputText(T"Syntax", plan.."-syntax", details["syntax"], input_attributes))
          ngx.print(ui_helper.createInputSelect(T"Selection Mode", plan.."-selection_mode", selection_modes, details["selection_mode"], select_attributes))
          ngx.print(ui_helper.createInputText(T"End of Number (EoN)", plan.."-eon", details["eon"], eon_attributes))
          ngx.print(ui_helper.createInputSelect(T"No Match Action", plan.."-no_match_action", no_match_actions, details["no_match_action"], select_attributes))
          ngx.print(ui_helper.createSwitch(T"No Match Includes EoN", plan.."-no_match_include_eon", details["no_match_include_eon"], switch_attributes))
          ngx.print(ui_helper.createInputText(T"Open Inter-digit Timer", plan.."-open_inter_digit_timer", details["open_inter_digit_timer"], timer_attributes))
          ngx.print(ui_helper.createInputText(T"Closed Inter-digit Timer", plan.."-closed_inter_digit_timer", details["closed_inter_digit_timer"], timer_attributes))
          ngx.print(ui_helper.createLabel(T"Network", details["network"], input_attributes))
        end
      ngx.print('\
    </fieldset>\
    <fieldset>\
      <legend>'); ngx.print(T"Dial Plan Entries"); ngx.print('</legend>\
        ')
        for _,p in ipairs(plans) do
          local name = p[1]
          local plan = "dial_plan_" .. name
          local entry_filter = function(data)
            if data.dial_plan == plan then
              return true
            end
            return false
          end
          local entry_options = {
            canEdit = true,
            canAdd = true,
            canDelete = true,
            tableid = name.."-dialplanentries",
            basepath = "uci.mmpbx.dial_plan_entry.",
            sorted = "pattern",
          }
          local entry_data, entry_helpmsg = post_helper.handleTableQuery(entry_columns, entry_options, entry_filter, nil, entry_valid)
          local entry_attributes = {
            group = {
              class ="monitor-dial_plan monitor-"..name.." monitor-1",
            },
          }
          ngx.print(ui_helper.createTable(entry_columns, entry_data, entry_options, entry_attributes, entry_helpmsg))
        end
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
<div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>\
'); ngx.print(ui_helper.createFooter()); ngx.print('\
<script>\
$(document).ready(function() {\
    $("#save-config").attr("id","save-form");\
    $("#save-form").click(function(){\
      $(".loading-wrapper").removeClass("hide");\
      var selected_plan = $("#dial_plan").val();\
      var form = $(".modal form");\
      var params = $("#dial_plan_config").serializeArray();\
      params.push({\
        name : "dial_plan",\
        value : selected_plan\
      }, {\
        name : "action",\
        value : "SAVE"\
      }, {\
        name : "fromModal",\
        value : "YES"\
      }, tch.elementCSRFtoken());\
      var target = form.attr("action");\
      tch.loadModal(target, params, function (){});\
    });\
});\
</script>\
');