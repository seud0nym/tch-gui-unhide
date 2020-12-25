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
}
content_helper.getExactContent(settings)

local html = {}

html[#html+1] = format(stylesheet, "icons-" .. settings["ICONS"], ".css")
html[#html+1] = format(stylesheet, "common-override", ".css")

if settings["THEME"]:find("^telstra") then
  html[#html+1] = format(favicon, "telstra/")
  html[#html+1] = format(stylesheet, "telstra", "/colour-text.css")
  if settings["THEME"] == "telstramodern" then
    html[#html+1] = format(stylesheet, "common-advanced-override", ".css")
    settings["THEME"] = "telstra"
  end
else
  html[#html+1] = format(stylesheet, "common-advanced-override", ".css")
  html[#html+1] = format(favicon, "")
  if settings["COLOR"] ~= "monochrome" then
    html[#html+1] = format(stylesheet, "colour-" .. settings["COLOR"], ".css")
  end
  html[#html+1] = format(stylesheet, settings["THEME"], "/colour-text.css")
  if settings["COLOR"] == "monochrome" then
    html[#html+1] = format(stylesheet, settings["THEME"], "/colour-monochrome.css")
  end
end
html[#html+1] = format(stylesheet, settings["THEME"], "/advanced-override.css")

ngx.print(html)
