--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local theme_helper = require("theme-schedule_helper")
local ui_helper = require("web.ui_helper")
local modal_helper = require("theme-modal_helper")
local match = string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean

local mapParams = {
  theme_name = "uci.tch_gui_unhide.theme.name",
  theme_colour = "uci.tch_gui_unhide.theme.colour",
  card_icons = "uci.tch_gui_unhide.theme.icons",
}
local mapValid = {
  theme_name = gVIES(modal_helper.theme_names),
  theme_colour = gVIES(modal_helper.theme_colours),
  card_icons = gVIES(modal_helper.card_icons_options),
}

local cards = {}
for _,v in ipairs(proxy.getPN("uci.web.card.",true)) do
  local name = match(v.path,"uci%.web%.card%.@card_([^%.]*)%.")
  local title = modal_helper.card_titles[name] or name
  local hide = v.path.."hide"
  local values = proxy.get(v.path.."card",hide)
  cards[#cards+1] = { untaint(values[1].value),name,title,untaint(values[2].value) }
  mapParams[name] = hide
  mapValid[name] = vB
end

local theme_settings,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if not helpmsg then
  helpmsg = {}
end

table.sort(cards,function(a,b)
  if a[4] == b[4] then
    return a[1] < b[1]
  end
  return a[4] < b[4]
end)

local light,night = theme_helper.getThemeSchedule()
local on_schedule = "0"
if light.time and night.time and light.enabled and night.enabled then
  on_schedule = "1"
end
if not light.time then
  light.time = "6:0"
end
if not night.time then
  night.time = "18:0"
end

if ngx.var.request_method == "POST" then
  local vars = ngx.req.get_post_args()
  if vars["action"] == "SAVE" then
    local new_on_schedule = untaint(vars["on_schedule"])
    local new_light_time = untaint(vars["light_time"])
    local new_night_time = untaint(vars["night_time"])
    if new_on_schedule ~= on_schedule or new_light_time ~= light.time or new_night_time ~= night.time then
      on_schedule = new_on_schedule
      light.time,light.hh,light.mm = theme_helper.updateCronEntry("light",light.path,new_light_time,on_schedule)
      night.time,night.hh,night.mm = theme_helper.updateCronEntry("night",night.path,new_night_time,on_schedule)
      if on_schedule == "1" then
        local now = tonumber(os.date("%H")) * 60 + tonumber(os.date("%M"))
        local light_time = tonumber(light.hh) * 60 + tonumber(light.mm)
        local night_time = tonumber(night.hh) * 60 + tonumber(night.mm)
        if light_time <= night_time then
          if now >= light_time and now <= night_time then
            proxy.set("uci.tch_gui_unhide.theme.name","light")
          else
            proxy.set("uci.tch_gui_unhide.theme.name","night")
          end
        else
          if now >= night_time and now <= light_time then
            proxy.set("uci.tch_gui_unhide.theme.name","night")
          else
            proxy.set("uci.tch_gui_unhide.theme.name","light")
          end
        end
      end
      proxy.set("rpc.gui.cron.writecrontab","1")
    end

    local gateway = untaint(vars["gateway"])
    if gateway == "1" then
      proxy.set("uci.web.card.@card_CPU.hide","1")
      proxy.set("uci.web.card.@card_RAM.hide","1")
    end

    local broadband = untaint(vars["broadband"])
    if broadband == "1" then
      proxy.set("uci.web.card.@card_WANDown.hide","1")
      proxy.set("uci.web.card.@card_WANUp.hide","1")
    end

    if gateway == "1" and broadband == "1" then
      proxy.set("uci.web.card.@card_Charts.hide","1")
    end
  end
end

local scheduled_attr = {
  group = {
    class = "monitor-on_schedule monitor-1"
  },
}
local alert_info = {
  alert = {
    class = "alert alert-info"
  },
}
local alert_warn = {
  alert = {
    class = "alert alert-warn"
  },
}

message_helper.pushMessage("Successful theme changes will be visible <b>after</b> you save and close this window.","info")

ngx.print(ui_helper.createHeader(T"Management",false,false),'\
  <div class="modal-body update">');
    lp.include("tabs-management.lp")
    ngx.print('\
    <form class="form-horizontal" method="post" action="modals/theme-modal.lp">'
      ,ui_helper.createMessages(message_helper.popMessages()),'\
      <fieldset>\
        <legend>',T"Theme Selection",'</legend>',
          ui_helper.createSwitch(T"Scheduled Change","on_schedule",on_schedule,{ input = { class = "monitor-changes" } }),
          ui_helper.createInputSelect(T"Light Theme Start","light_time",modal_helper.times,light.time,scheduled_attr),
          ui_helper.createInputSelect(T"Night Theme Start","night_time",modal_helper.times,night.time,scheduled_attr),
          ui_helper.createInputSelect(T"Base Theme","theme_name",modal_helper.theme_names,theme_settings["theme_name"],{ group = { class = "monitor-on_schedule monitor-0" }, },helpmsg["theme_name"]),
          ui_helper.createInputSelect(T"Highlight Colour","theme_colour",modal_helper.theme_colours,theme_settings["theme_colour"],nil,helpmsg["theme_colour"]),
          ui_helper.createInputSelect(T"Card Icons","card_icons",modal_helper.card_icons_options,theme_settings["card_icons"],nil,helpmsg["card_icons"]))
          local landing_page_state = proxy.get("uci.tch_gui_unhide.theme.landing_page_state")
          if landing_page_state then
            if landing_page_state[1].value == "debranded" and (theme_settings.theme_name == "telstra" or theme_settings.theme_name == "telstramodern") then
              ngx.print(ui_helper.createAlertBlock(T"The landing (login) page will still be de-branded even though you have selected a non-Telstra theme. You must re-run tch-gui-unhide with the -ly option to restore Telstra branding.",alert_warn))
            elseif landing_page_state[1].value == "telstra" and not (theme_settings.theme_name == "telstra" or theme_settings.theme_name == "telstramodern") then
              ngx.print(ui_helper.createAlertBlock(T"The landing (login) page will still be Telstra-branded even though you have selected a non-Telstra theme. You must re-run tch-gui-unhide with the -ln option to remove Telstra branding.",alert_warn))
            end
          end
          ngx.print('\
      </fieldset>\
      <fieldset>\
        <legend>',T"Card Visibility",'</legend>',
          ui_helper.createAlertBlock(T"The CPU and RAM charts depend on the "..modal_helper.card_titles.gateway.." card,and the WAN charts depend on the "..modal_helper.card_titles.broadband.." card. Hiding the base cards will also hide their dependents.",alert_info),
          '<fieldset>')
          for _,v in pairs(cards) do
            ngx.print(ui_helper.createSwitch(T(v[3]),v[2],theme_settings[v[2]],{ group = { class = "card-visibility-switch" }, values = { off = "1", on = "0" } }))
          end
          ngx.print('</fieldset>',
          ui_helper.createAlertBlock(T"You can also adjust card visibility and sequencing using the <b>tch-gui-unhide-cards</b> command line utility.",alert_info),'\
      </fieldset>\
    </form>\
  </div>\
  <script>\
  $(document).ready(function () {\
    let theme_name=$("#theme_name").val();\
    if(theme_name.startsWith("telstra")||theme_name=="classic") {\
      $("#theme_colour").val("blue").prop("disabled",true);\
    }\
    $("#on_schedule").change(function() {\
      if ($(this).val()=="1") {\
        $("#theme_name").val("light");\
        $("#theme_colour").prop("disabled",false);\
      }\
    });\
    $("#theme_name").change(function() {\
      let name=$(this).val();\
      if (name.startsWith("telstra")||name=="classic") {\
        $("#theme_colour").val("blue").prop("disabled",true);\
      } else {\
        $("#theme_colour").prop("disabled",false);\
      }\
    });\
  });\
  </script>\
',ui_helper.createFooter());