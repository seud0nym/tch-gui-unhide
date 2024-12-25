--pretranslated: do not change this file

gettext.textdomain('webui-core')

local isBridgedMode = require("bridgedmode_helper").isBridgedMode()
local ipairs,string = ipairs,string
local format,len,sub = string.format,string.len,string.sub

local items = {
  {"dns-modal.lp",T"Configuration"},
  {"dns-interfaces-modal.lp",T"WAN Interfaces"},
  {"dns-interceptd-modal.lp",T"Intercept Daemon"},
}
if isBridgedMode then
  items[2][2] = T"LAN Interfaces"
else
  items[#items+1] = {"firewall-dns_int-modal.lp",T"Hijacking"}
end

local uri = ngx.var.uri
if ngx.var.args and len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

local html = {}
html[#html+1] = '<ul class="nav nav-tabs">'
for _,v in ipairs(items) do
  local active = ""
  local modal = "/modals/"..v[1]
  if sub(uri,1,len(modal)) == modal then
    active = "active"
  end
  html[#html+1] = format('<li class="%s"><a href="#" data-remote="/modals/%s">%s</a></li>',active,v[1],v[2])
end
html[#html+1] = '</ul>'

ngx.print(html)
