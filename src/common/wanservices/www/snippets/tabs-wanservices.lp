--pretranslated: do not change this file
local proxy = require("datamodel")

gettext.textdomain('webui-core')

local items = {
  {"wanservices-ddns-modal.lp",T"Dynamic DNS"},
  {"wanservices-dmz-modal.lp",T"DMZ"},
}

if io.open("/lib/functions/firewall-wol.sh","r") and proxy.get("uci.wol.config.") then
  items[#items+1] = {"wanservices-wol-modal.lp",T"WoL Over Internet"}
end

items[#items+1] = {"wanservices-upnp-modal.lp",T"UPnP"}

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

local html = {}

html[#html+1] = '<ul class="nav nav-tabs">'
for _,v in ipairs(items) do
  local active = ""
  local modal = "/modals/"..v[1]
  if string.sub(uri,1,string.len(modal)) == modal then
    active = "active"
  end
  html[#html+1] = string.format('<li class="%s"><a href="#" data-remote="/modals/%s">%s</a></li>',active,v[1],v[2])
end
html[#html+1] = '</ul>'

ngx.print(html)

