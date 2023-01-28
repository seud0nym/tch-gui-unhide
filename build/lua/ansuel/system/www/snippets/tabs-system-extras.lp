--pretranslated: do not change this file
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ipairs,string = ipairs,string
local format,len,sub = string.format,string.len,string.sub

local items = {
  {"system-reboot-modal.lp",T"Advanced Reboot"},
  {"system-ssh-modal.lp",T"SSH"},
  {"system-syslog-modal.lp",T"syslog"},
}

local iperf_available = proxy.get("uci.iperf.iperf.enabled")
if iperf_available then
  items[#items+1] = {"system-iperf-modal.lp",T"iperf"}
end

local tabs = {}
local session = ngx.ctx.session
for _,v in pairs(items) do
  if session:hasAccess("/modals/"..v[1]) then
    tabs[#tabs+1] = v
  end
end

local uri = ngx.var.uri
if ngx.var.args and len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

local html = {}

for _,v in ipairs(tabs) do
  local active = ""
  local modal = "/modals/"..v[1]
  if sub(uri,1,len(modal)) == modal then
    active = "active"
  end
  html[#html+1] = format('<li class="%s"><a id ="%s" href="#" class="open-modal" data-remote="/modals/%s">%s</a></li>',active,v[2],v[1],v[2])
end

ngx.print('<ul class="nav nav-tabs">',html,'</ul><script src="/js/modal-busy.js"></script>');
