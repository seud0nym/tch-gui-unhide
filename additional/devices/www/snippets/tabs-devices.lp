--pretranslated: do not change this file

gettext.textdomain('webui-core')

local ipairs, string, table = ipairs, string, table
local proxy = require("datamodel")

local wan_mode = proxy.get("uci.network.config.wan_mode")
if not wan_mode or wan_mode[1].value ~= "bridge" then

local items = {
  {"device-modal.lp", T"Device List"},
  {"device-map-modal.lp", T"Network Map"}
}

ngx.print('<ul class="nav nav-tabs">\
');  

local html = {}

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
    uri = uri .. "?" .. ngx.var.args
end

for _,v in ipairs(items) do
    local active = ""
    if uri == ("/modals/" .. v[1]) then
        active = "active"
    end

    html[#html+1] = string.format('<li class="%s"><a href="#" data-remote="/modals/%s">%s</a></li>', active, v[1], v[2])
end
ngx.print(html)

  ngx.print('\
\
</ul>\
'); 
end
