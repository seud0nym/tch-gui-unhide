--pretranslated: do not change this file

gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ipairs, string, table = ipairs, string, table
local format, len, sub = string.format, string.len, string.sub

local items = {
  {"firewall-modal.lp", T"Configuration"},
  {"firewall-zones-modal.lp", T"Zones"},
  {"firewall-user-rules-modal.lp", T"User Defined Rules"},
  {"firewall-rules-modal.lp", T"System Rules"},
  {"firewall-port-forwarding-modal.lp", T"Port Forwarding"},
  {"firewall-cone-modal.lp", T"Cone NAT"},
}

local nat_alg_card_hidden = proxy.get("uci.web.card.@card_natalghelper.hide")
if nat_alg_card_hidden and nat_alg_card_hidden[1] and nat_alg_card_hidden[1].value == "1" then
  items[#items+1] = {"nat-alg-helper-modal.lp", T"NAT Helpers"}
end

local html = {}

local uri = ngx.var.uri
if ngx.var.args and len(ngx.var.args) > 0 then
  uri = uri .. "?" .. ngx.var.args
end

html[#html+1] = '<ul class="nav nav-tabs">'
for _,v in ipairs(items) do
  local active = ""
  local modal = "/modals/" .. v[1]
  if sub(uri,1,len(modal)) == modal then
    active = "active"
  end
  html[#html+1] = format('<li class="%s"><a href="#" data-remote="/modals/%s">%s</a></li>', active, v[1], v[2])
end
html[#html+1] = '</ul>'

ngx.print(html)
