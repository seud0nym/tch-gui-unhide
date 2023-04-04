--pretranslated: do not change this file
gettext.textdomain('webui-core')

local ipairs,string = ipairs,string
local format,length,substring = string.format,string.len,string.sub
local proxy = require("datamodel")

local dhcp = false
for _,v in ipairs(proxy.getPN("uci.dhcp.dhcp.", true)) do
  local server = proxy.get(v.path.."dhcpv4",v.path.."dhcpv6")
  if server and (server[1].value == "server" or server[2].value == "server") then
    dhcp = true
    break
  end
end

local adguard = false
if not dhcp then
  local adguard_active = proxy.get("rpc.gui.init.files.@AdGuardHome.active")
  if adguard_active and adguard_active[1].value == "1" then
    adguard = true
  end
end

local items = {
  {"ethernet-modal.lp",T"Configuration"},
}
if dhcp and not adguard then
  items[#items+1] = {"static-leases-modal.lp",T"Static Leases"}
end

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

ngx.print('<ul class="nav nav-tabs">');
for _,v in ipairs(items) do
  local active = ""
  local modal = "/modals/"..v[1]
  if substring(uri,1,length(modal)) == modal then
    active = "active"
  end
  ngx.print(format('<li class="%s"><a href="#" data-remote="/modals/%s">%s</a></li>',active,v[1],v[2]))
end
ngx.print('</ul>');