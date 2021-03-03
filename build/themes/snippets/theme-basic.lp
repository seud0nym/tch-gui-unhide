--pretranslated: do not change this file

gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local format = string.format
local favicon = '<link href="/img/%sfavicon.ico" rel="shortcut icon" type="image/x-icon"/>'
local stylesheet = '<link href="/css/%s%s" rel="stylesheet">'

local settings = {
  THEME = "rpc.gui.theme.THEME",
  COLOR = "rpc.gui.theme.COLOR",
  ICONS = "rpc.gui.theme.ICONS",
  KEEPLP = "rpc.gui.theme.KEEPLP",
}
content_helper.getExactContent(settings)

local html = {}

html[#html+1] = format(stylesheet, "icons-" .. settings["ICONS"], ".css")
html[#html+1] = format(stylesheet, "common-override", ".css")

if settings["THEME"]:find("^telstra") then
  html[#html+1] = format(favicon, "telstra/")
  html[#html+1] = format(stylesheet, "telstra", "/colour-text.css")
else
  html[#html+1] = format(stylesheet, "common-basic-override", ".css")
  html[#html+1] = format(favicon, "")
  if settings["COLOR"] ~= "monochrome" then
    html[#html+1] = format(stylesheet, "colour-" .. settings["COLOR"], ".css")
  end
  html[#html+1] = format(stylesheet, settings["THEME"], "/colour-text.css")
  if settings["COLOR"] == "monochrome" then
    html[#html+1] = format(stylesheet, settings["THEME"], "/colour-monochrome.css")
  end
  html[#html+1] = format(stylesheet, settings["THEME"], "/basic-override.css")
end

if settings["KEEPLP"] == "y" then
  if settings["THEME"] == "night" then
    html[#html+1] = "<style>"
    html[#html+1] = "body.landingpage{color:#3B3B3B;background-color:#F8F8F8;background-image:-moz-linear-gradient(top, #f4f4f4 0%, #dadada 100%);background-image:-webkit-gradient(linear, left top, left bottom, color-stop(0%, #f4f4f4), color-stop(100%, #dadada));background-image:-webkit-linear-gradient(top, #f4f4f4 0%, #dadada 100%);background-image:-ms-linear-gradient(top, #f4f4f4 0%, #dadada 100%);background-image:-o-linear-gradient(top, #f4f4f4 0%, #dadada 100%);filter:progid:DXImageTransform.Microsoft.gradient(startColorstr='#F4F4F4', endColorstr='#DADADA', GradientType=1);background-image:linear-gradient(top, #f4f4f4 0%, #dadada 100%);}"
    html[#html+1] = "</style>"
  end
else
  if settings["THEME"] == "light" then
    html[#html+1] = "<style>"
    html[#html+1] = "@media screen and (max-width: 979px) body.landingpage{background-color:#f8f8f8;background-image:unset;}"
    html[#html+1] = "body.landingpage{background-color:#f8f8f8;background-image:unset;}"
    html[#html+1] = "</style>"
  elseif settings["THEME"] == "night" then
    html[#html+1] = "<style>"
    html[#html+1] = "@media screen and (max-width: 979px) body.landingpage{background-color:#353c42;background-image:unset;}"
    html[#html+1] = "body.landingpage{background-color:#353c42;background-image:unset;}"
    html[#html+1] = "body.landingpage#detailed_info_mobile>div:nth-child(5)>form>center>div{background-color:#f8f8f8;}"
    html[#html+1] = "</style>"
  else -- Telstra
    html[#html+1] = "<style>"
    html[#html+1] = ".lp-logo{display:block;-moz-box-sizing:border-box;box-sizing:border-box;background:url(/img/light/logo.gif) no-repeat;width:240px;height:93px;padding-left:240px;background-size:240px;}"
    html[#html+1] = "</style>"
  end
end

ngx.print(html)
