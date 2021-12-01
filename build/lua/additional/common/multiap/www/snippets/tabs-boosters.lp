--pretranslated: do not change this file
gettext.textdomain('webui-core')

local ipairs,string = ipairs,string
local proxy = require("datamodel")

local items = {
  {"wireless-boosters-modal.lp",T"MultiAP Status"},
}

local controller_state = proxy.get("uci.multiap.controller.enabled")
if controller_state and controller_state[1] and controller_state[1].value == "1" then
  items[#items+1] = {"wireless-boosters-status-modal.lp",T"Booster Status"}
  items[#items+1] = {"wireless-boosters-boosters-modal.lp",T"Boosters"}
  items[#items+1] = {"wireless-boosters-devices-modal.lp",T"Wi-Fi Devices"}
end

local html = {}

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

for _,v in ipairs(items) do
  local active = ""
  if uri == ("/modals/"..v[1]) then
    active = "active"
  end
  html[#html+1] = string.format('<li class="%s"><a href="#" data-remote="/modals/%s">%s</a></li>',active,v[1],v[2])
end

ngx.print('<ul class="nav nav-tabs">\
');  
ngx.print(html)
ngx.print('\
</ul>\
'); 