--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local theme_helper = require("theme-schedule_helper")
local ui_helper = require("web.ui_helper")
local gsub, match, untaint = string.gsub, string.match, string.untaint
local gVIES = post_helper.getValidateInEnumSelect
local vB =post_helper.validateBoolean

local theme_names = {
    {"light", T"Light"},
    {"night", T"Night"},
    {"telstra", T"Telstra Branding (Classic)"},
    {"telstramodern", T"Telstra Branding (Modern)"},
}
local theme_colours = {
    {"blue", T"Blue"},
    {"green", T"Green"},
    {"orange", T"Orange"},
    {"purple", T"Purple"},
    {"red", T"Red"},
    {"monochrome", T"Monochrome"},
    {"MONOCHROME", T"Monochrome (with Monochrome charts)"},
}
local card_icons_options = {
    {"visible", T"Visible"},
    {"hidden", T"Hidden"},
}
local times = {
  {"0:0", T"Midnight"},
  {"0:15", T"12:15am"},
  {"0:30", T"12:30am"},
  {"0:45", T"12:45am"},
  {"1:0", T"1:00am"},
  {"1:15", T"1:15am"},
  {"1:30", T"1:30am"},
  {"1:45", T"1:45am"},
  {"2:0", T"2:00am"},
  {"2:15", T"2:15am"},
  {"2:30", T"2:30am"},
  {"2:45", T"2:45am"},
  {"3:0", T"3:00am"},
  {"3:15", T"3:15am"},
  {"3:30", T"3:30am"},
  {"3:45", T"3:45am"},
  {"4:0", T"4:00am"},
  {"4:15", T"4:15am"},
  {"4:30", T"4:30am"},
  {"4:45", T"4:45am"},
  {"5:0", T"5:00am"},
  {"5:15", T"5:15am"},
  {"5:30", T"5:30am"},
  {"5:45", T"5:45am"},
  {"6:0", T"6:00am"},
  {"6:15", T"6:15am"},
  {"6:30", T"6:30am"},
  {"6:45", T"6:45am"},
  {"7:0", T"7:00am"},
  {"7:15", T"7:15am"},
  {"7:30", T"7:30am"},
  {"7:45", T"7:45am"},
  {"8:0", T"8:00am"},
  {"8:15", T"8:15am"},
  {"8:30", T"8:30am"},
  {"8:45", T"8:45am"},
  {"9:0", T"9:00am"},
  {"9:15", T"9:15am"},
  {"9:30", T"9:30am"},
  {"9:45", T"9:45am"},
  {"10:0", T"10:00am"},
  {"10:15", T"10:15am"},
  {"10:30", T"10:30am"},
  {"10:45", T"10:45am"},
  {"11:0", T"11:00am"},
  {"11:15", T"11:15am"},
  {"11:30", T"11:30am"},
  {"11:45", T"11:45am"},
  {"12:0", T"12:00pm"},
  {"12:15", T"12:15pm"},
  {"12:30", T"12:30pm"},
  {"12:45", T"12:45pm"},
  {"13:0", T"1:00pm"},
  {"13:15", T"1:15pm"},
  {"13:30", T"1:30pm"},
  {"13:45", T"1:45pm"},
  {"14:0", T"2:00pm"},
  {"14:15", T"2:15pm"},
  {"14:30", T"2:30pm"},
  {"14:45", T"2:45pm"},
  {"15:0", T"3:00pm"},
  {"15:15", T"3:15pm"},
  {"15:30", T"3:30pm"},
  {"15:45", T"3:45pm"},
  {"16:0", T"4:00pm"},
  {"16:15", T"4:15pm"},
  {"16:30", T"4:30pm"},
  {"16:45", T"4:45pm"},
  {"17:0", T"5:00pm"},
  {"17:15", T"5:15pm"},
  {"17:30", T"5:30pm"},
  {"17:45", T"5:45pm"},
  {"18:0", T"6:00pm"},
  {"18:15", T"6:15pm"},
  {"18:30", T"6:30pm"},
  {"18:45", T"6:45pm"},
  {"19:0", T"7:00pm"},
  {"19:15", T"7:15pm"},
  {"19:30", T"7:30pm"},
  {"19:45", T"7:45pm"},
  {"20:0", T"8:00pm"},
  {"20:15", T"8:15pm"},
  {"20:30", T"8:30pm"},
  {"20:45", T"8:45pm"},
  {"21:0", T"9:00pm"},
  {"21:15", T"9:15pm"},
  {"21:30", T"9:30pm"},
  {"21:45", T"9:45pm"},
  {"22:0", T"10:00pm"},
  {"22:15", T"10:15pm"},
  {"22:30", T"10:30pm"},
  {"22:45", T"10:45pm"},
  {"23:0", T"11:00pm"},
  {"23:15", T"11:15pm"},
  {"23:30", T"11:30pm"},
  {"23:45", T"11:45pm"},
}
local card_titles = {
  adblock = "Adblock",
  broadband = "Broadband",
  contentsharing = "Content Sharing",
  cwmpconf = "CWMP",
  Devices = "Devices",
  diagnostics = "Diagnostics",
  eco = "Eco Settings",
  firewall = "Firewall",
  fon = "Telstra AIR",
  gateway = gsub(untaint(proxy.get("uci.env.var.variant_friendly_name")[1].value), "TLS", ""),
  internet = "Internet Access",
  iproutes = "IP Routing",
  LAN = "Local Network",
  lte = "Mobile",
  natalghelper = "NAT Helpers",
  opkg = "Packages",
  parental = "Parental Controls",
  printersharing = "Printer Sharing",
  qos = "QoS",
  relaysetup = "Relay Setup",
  speedservice = "Speed Test",
  system = "System Extras",
  telephony = "Telephony",
  tod = "Time of Day",
  usermgr = "Management",
  WANDown = "WAN Download",
  wanservices = "WAN Services",
  WANUp = "WAN Upload",
  wireless = "Wi-Fi",
  wireless_boosters = "Wi-Fi Boosters",
  xdsl = "xDSL Config",
}

local mapParams = {
  theme_name = "rpc.gui.theme.THEME",
  theme_colour = "rpc.gui.theme.COLOR",
  card_icons = "rpc.gui.theme.ICONS",
}
local mapValid = {
  theme_name = gVIES(theme_names),
  theme_colour = gVIES(theme_colours),
  card_icons = gVIES(card_icons_options),
}

local cards = {}
local v
for _,v in ipairs(proxy.getPN("uci.web.card.", true)) do
  local name = match(v.path,"uci%.web%.card%.@card_([^%.]*)%.")
  local title = card_titles[name] or name
  local hide = v.path.."hide"
  local values = proxy.get(v.path.."card", hide)
  cards[#cards+1] = { untaint(values[1].value), name, title, untaint(values[2].value) }
  mapParams[name] = hide
  mapValid[name] = vB
end

local theme_settings, helpmsg = post_helper.handleQuery(mapParams, mapValid)

table.sort(cards, function(a,b)
  if a[4] == b[4] then
    return a[1] < b[1]
  end
  return a[4] < b[4]
end)

local light, night = theme_helper.getThemeSchedule()
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
      light.time, light.hh, light.mm = theme_helper.updateCronEntry("light", light.path, new_light_time, on_schedule)
      night.time, night.hh, night.mm = theme_helper.updateCronEntry("night", night.path, new_night_time, on_schedule)
      if on_schedule == "1" then
        local now = tonumber(os.date("%H")) * 60 + tonumber(os.date("%M"))
        local light_time = tonumber(light.hh) * 60 + tonumber(light.mm)
        local night_time = tonumber(night.hh) * 60 + tonumber(night.mm)
        if light_time <= night_time then
          if now >= light_time and now <= night_time then
            proxy.set("rpc.gui.theme.THEME","light")
          else
            proxy.set("rpc.gui.theme.THEME","night")
          end
        else
          if now >= night_time and now <= light_time then
            proxy.set("rpc.gui.theme.THEME","night")
          else
            proxy.set("rpc.gui.theme.THEME","light")
          end
        end
      end
      proxy.set("rpc.gui.cron.writecrontab","1")
    end

    local gateway = untaint(vars["gateway"])
    if gateway == "1" then
      proxy.set("uci.web.card.@card_CPU.hide", "1")
      proxy.set("uci.web.card.@card_RAM.hide", "1")
    end

    local broadband = untaint(vars["broadband"])
    if broadband == "1" then
      proxy.set("uci.web.card.@card_WANDown.hide", "1")
      proxy.set("uci.web.card.@card_WANUp.hide", "1")
    end
  end
end

message_helper.pushMessage("Successful theme changes will be visible <b>after</b> you save and close this window.", "info")

ngx.print('\
');  ngx.print(ui_helper.createHeader(T"Management", false, false))   ngx.print('\
  <div class="modal-body update">\
  ');
    local lp = require("web.lp")
    lp.setpath("/www/snippets/")
    lp.include("tabs-management.lp")
    ngx.print('\
    <form class="form-horizontal" method="post" action="modals/theme-modal.lp">\
    ');
      ngx.print(ui_helper.createMessages(message_helper.popMessages()))
      ngx.print('\
      <fieldset>\
        <legend>');  ngx.print( T"Theme Selection" ); ngx.print('</legend>\
        ');
          local schedule_attr = {
            input = {
              class = "monitor-changes"
            }
          }
          local scheduled_attr = {
            group = {
              class = "monitor-on_schedule monitor-1"
            },
          }
          local not_scheduled_attr = {
            group = {
              class = "monitor-on_schedule monitor-0"
            },
          }

          local html = {}
          html[#html + 1] = ui_helper.createSwitch(T"Scheduled Change", "on_schedule", on_schedule, schedule_attr)
          html[#html + 1] = ui_helper.createInputSelect(T"Light Theme Start", "light_time", times, light.time, scheduled_attr)
          html[#html + 1] = ui_helper.createInputSelect(T"Night Theme Start", "night_time", times, night.time, scheduled_attr)
          html[#html + 1] = ui_helper.createInputSelect(T"Base Theme", "theme_name", theme_names, theme_settings["theme_name"], not_scheduled_attr, helpmsg["theme_name"])
          html[#html + 1] = ui_helper.createInputSelect(T"Highlight Colour", "theme_colour", theme_colours, theme_settings["theme_colour"], nil, helpmsg["theme_colour"])
          html[#html + 1] = ui_helper.createInputSelect(T"Card Icons", "card_icons", card_icons_options, theme_settings["card_icons"], nil, helpmsg["card_icons"])
          ngx.print(html)
          ngx.print('\
      </fieldset>\
      <fieldset>\
        <legend>');  ngx.print( T"Card Visibility" ); ngx.print('</legend>\
        ');
          local on_off_attr = {
            group = {
              class = "card-visibility-switch"
            },
            values = {
              off = "1",
              on = "0"
            }
          }
          local alert_info = {
            alert = {
              class = "alert alert-info"
            },
          }

          local html = {}
          html[#html + 1] = ui_helper.createAlertBlock(T"The CPU and RAM charts depend on the "..card_titles.gateway.." card, and the WAN charts depend on the "..card_titles.broadband.." card. Hiding the base cards will also hide their dependents.",alert_info)
          html[#html + 1] = '<fieldset>'
          for _,v in pairs(cards) do
            html[#html + 1] = ui_helper.createSwitch(T(v[3]), v[2], theme_settings[v[2]], on_off_attr)
          end
          html[#html + 1] = '</fieldset>'
          html[#html + 1] = ui_helper.createAlertBlock(T"You can also adjust card visibility and sequencing using the <b>tch-gui-unhide-cards</b> command line utility.",alert_info)
          ngx.print(html)
          ngx.print('\
      </fieldset>\
    </form>\
  </div>\
  <script>\
  $(document).ready(function () {\
    if ($("#theme_name").val().startsWith("telstra")) {\
      $("#theme_colour").val("blue");\
      $("#theme_colour").prop("disabled", true);\
    }\
    $("#on_schedule").change(function() {\
      if ($(this).val() == "1") {\
        $("#theme_name").val("light");\
        $("#theme_colour").prop("disabled", false);\
      }\
    });\
    $("#theme_name").change(function() {\
      if ($(this).val().startsWith("telstra")) {\
        $("#theme_colour").val("blue");\
        $("#theme_colour").prop("disabled", true);\
      }\
      else {\
        $("#theme_colour").prop("disabled", false);\
      }\
    });\
  });\
  </script>\
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
');