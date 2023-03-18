--pretranslated: do not change this file

gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local find,format,lower = string.find,string.format,string.lower
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local favicon = '<link href="/img/%sfavicon.ico" rel="shortcut icon" type="image/x-icon"/>'
local stylesheet = '<link href="/css/%s%s" rel="stylesheet">'

local settings = {
  THEME = "rpc.gui.theme.THEME",
  COLOR = "rpc.gui.theme.COLOR",
  ICONS = "rpc.gui.theme.ICONS",
}
content_helper.getExactContent(settings)

local html = {}
local theme = settings["THEME"]

html[#html+1] = format(stylesheet,"icons-" .. settings["ICONS"],".css")
html[#html+1] = format(stylesheet,"common-override",".css")

if find(theme,"^classic") then
  html[#html+1] = format(favicon,"")
  html[#html+1] = format(stylesheet,"telstra","/colour-text.css")
  html[#html+1] = format(stylesheet,"telstra","/tch-logo.css")
  theme = "telstra"
elseif find(theme,"^telstra") then
  html[#html+1] = format(favicon,"telstra/")
  html[#html+1] = format(stylesheet,"telstra","/colour-text.css")
  if theme == "telstramodern" then
    html[#html+1] = format(stylesheet,"common-advanced-override",".css")
    theme = "telstra"
  end
else
  html[#html+1] = format(stylesheet,"common-advanced-override",".css")
  html[#html+1] = format(favicon,"")
  if lower(settings["COLOR"]) ~= "monochrome" then
    html[#html+1] = format(stylesheet,"colour-" .. settings["COLOR"],".css")
  end
  html[#html+1] = format(stylesheet,theme,"/colour-text.css")
  if lower(settings["COLOR"]) == "monochrome" then
    html[#html+1] = format(stylesheet,theme,"/colour-monochrome.css")
  end
end
html[#html+1] = format(stylesheet,theme,"/advanced-override.css")

ngx.print(html)

local TGU_Config = ngx.shared.TGU_Config
local ok,err = TGU_Config:safe_set("THEME",untaint(settings["THEME"]))
if not ok then
  ngx.log(ngx.ERR,"Failed to store current theme: ",err)
end
ok,err = TGU_Config:safe_set("COLOR",untaint(settings["COLOR"]))
if not ok then
  ngx.log(ngx.ERR,"Failed to store current theme colour: ",err)
end
