--pretranslated: do not change this file
gettext.textdomain('webui-core')

local ipairs,string = ipairs,string
local format,length,substring = string.format,string.len,string.sub
local proxy = require("datamodel")

local dhcp = false
for _,v in ipairs(proxy.getPN("uci.dhcp.dhcp.", true)) do
  local server = proxy.get(v.path.."dhcpv4",v.path.."dhcpv6")
  if server and (server[1].value ~= "disabled" or server[2].value ~= "disabled") then
    dhcp = true
    break
  end
end

local items = {
  {"ethernet-global-modal.lp",T"Configuration"},
  {"ethernet-modal.lp",T"Interfaces"},
  {"ethernet-vlan-modal.lp",T"VLAN Filtering"},
}
if dhcp then
  items[#items+1] = {"ethernet-static-leases-modal.lp",T"Static Leases"}
end
items[#items+1] = {"device-modal.lp",T"Connected Devices"}
items[#items+1] = {"ethernet-mcsnooper-modal.lp",T"Multicast Snooping"}

local uri = ngx.var.uri
if uri == "/modals/ethernet-add-interface-modal.lp" then
  uri = "/modals/ethernet-modal.lp"
end
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

ngx.print('<ul class="nav nav-tabs">')
for _,v in ipairs(items) do
  local active = ""
  local modal = "/modals/"..v[1]
  if substring(uri,1,length(modal)) == modal then
    active = "active"
  end
  ngx.print(format('<li class="%s"><a href="#" data-remote="/modals/%s">%s</a></li>',active,v[1],v[2]))
end
ngx.print('</ul>')