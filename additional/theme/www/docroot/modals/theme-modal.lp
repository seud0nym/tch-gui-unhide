--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local ui_helper = require("web.ui_helper")
local gmatch, untaint = string.gmatch, string.untaint

local current_theme = content_helper.readfile("/usr/tch-gui-unhide.theme","line")
local theme_settings = {}
for k, v in gmatch(current_theme, "(%w+)=(%w+)") do
  theme_settings[k] = untaint(v)
end
local theme_names = {
    {"light", T"Light"},
    {"night", T"Night"},
    {"telstra", T"Telstra Branding"},
}
local theme_colours = {
    {"blue", T"Blue"},
    {"green", T"Green"},
    {"orange", T"Orange"},
    {"purple", T"Purple"},
    {"red", T"Red"},
    {"monochrome", T"Monochrome"},
}
local card_icons_options = {
    {"visible", T"Visible"},
    {"hidden", T"Hidden"},
}

if ngx.var.request_method == "POST" then
  local vars = ngx.req.get_post_args()
  if vars["action"] == "SAVE" then
    local theme_name = untaint(vars["theme_name"])
    local theme_colour = untaint(vars["theme_colour"])
    local card_icons = untaint(vars["card_icons"])
    if theme_name ~= theme_settings["THEME"] or theme_colour ~= theme_settings["COLOR"] or card_icons ~= theme_settings["ICONS"] then
      theme_settings["THEME"] = theme_name
      theme_settings["COLOR"] = theme_colour
      theme_settings["ICONS"] = card_icons
      local file, err_msg = io.open("/usr/tch-gui-unhide.theme", "w")
      if file then
        file:write("THEME=" .. theme_settings["THEME"] .. "; COLOR=" .. theme_settings["COLOR"] .. "; ICONS=" .. theme_settings["ICONS"] .. "; KEEPLP=" .. theme_settings["KEEPLP"] .. ";")
        file:close()
        message_helper.pushMessage("Changes successfully saved.", "success")
        message_helper.pushMessage("Theme change will be visible when you close this window.", "info")
      else
        message_helper.pushMessage("Failed to persist theme preferences: " .. err_msg, "error")
      end
    end
  end
end

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
          local html = {}
          html[#html + 1] = ui_helper.createInputSelect(T"Base Theme", "theme_name", theme_names, theme_settings["THEME"])
          html[#html + 1] = ui_helper.createInputSelect(T"Highlight Colour", "theme_colour", theme_colours, theme_settings["COLOR"])
          html[#html + 1] = ui_helper.createInputSelect(T"Card Icons", "card_icons", card_icons_options, theme_settings["ICONS"])
          ngx.print(html)
          ngx.print('\
      </fieldset>\
    </form>\
  </div>\
  <script>\
  $(document).ready(function () {\
    $("#theme_name").change(function () {\
      if ($(this).val() == "telstra") {\
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