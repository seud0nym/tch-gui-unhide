--pretranslated: do not change this file

gettext.textdomain('webui-core')

local bridged = require("bridgedmode_helper")
local ipairs,string = ipairs,string
local format,length,substring = string.format,string.len,string.sub

local items = {
  {"device-modal.lp",T"All Devices"},
  {"wifi-devices.lp",T"Wi-Fi Devices"},
}
if not bridged.isBridgedMode() then
  items[#items+1] = {"device-bwstats-modal.lp",T"Bandwidth Monitor"}
end

local uri = ngx.var.uri
if ngx.var.args and length(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

local html = { '<ul class="nav nav-tabs">' }
for _,v in ipairs(items) do
  local active = ""
  local modal = "/modals/"..v[1]
  if substring(uri,1,length(modal)) == modal then
    active = "active"
  end
  html[#html+1] = format('<li class="%s"><a href="#" data-remote="/modals/%s?dev=1">%s</a></li>',active,v[1],v[2])
end
html[#html+1] = '</ul>'
ngx.print(html);
