--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local gmatch, untaint = string.gmatch, string.untaint
local gVIES = post_helper.getValidateInEnumSelect

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
}
local card_icons_options = {
    {"visible", T"Visible"},
    {"hidden", T"Hidden"},
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
local theme_settings, helpmsg = post_helper.handleQuery(mapParams, mapValid)

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
          local html = {}
          html[#html + 1] = ui_helper.createInputSelect(T"Base Theme", "theme_name", theme_names, theme_settings["theme_name"], nil, helpmsg["theme_name"])
          html[#html + 1] = ui_helper.createInputSelect(T"Highlight Colour", "theme_colour", theme_colours, theme_settings["theme_colour"], nil, helpmsg["theme_colour"])
          html[#html + 1] = ui_helper.createInputSelect(T"Card Icons", "card_icons", card_icons_options, theme_settings["card_icons"], nil, helpmsg["card_icons"])
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
    $("#theme_name").change(function () {\
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