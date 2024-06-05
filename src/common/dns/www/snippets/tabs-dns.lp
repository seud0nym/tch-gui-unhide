--pretranslated: do not change this file

gettext.textdomain('webui-core')

local dns_helper = require("dns_helper")
local isBridgedMode = require("bridgedmode_helper").isBridgedMode()
local proxy = require("datamodel")
local ipairs,string = ipairs,string
local format,len,sub = string.format,string.len,string.sub
local items = {}

local mainport = proxy.get(dns_helper.dnsmasq_path.."port")[1].value

if mainport ~= "0" then
  items[#items+1] = {"dns-modal.lp",T"Configuration"}
  if not isBridgedMode then
    items[#items+1] = {"dns-rewrites-modal.lp",T"Rewrites"}
  end
end
items[#items+1] = {"dns-interceptd-modal.lp",T"Intercept Daemon"}
if not isBridgedMode then
  items[#items+1] = {"firewall-dns_int-modal.lp",T"Hijacking"}
  items[#items+1] = {"dns-rebind-modal.lp",T"Rebind Protection"}
end

local html = {}

local uri = ngx.var.uri
if ngx.var.args and len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

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
