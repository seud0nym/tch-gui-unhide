--pretranslated: do not change this file
gettext.textdomain('webui-core')
local content_helper = require("web.content_helper")
local ipairs, string, table = ipairs, string, table

local items = {
    {"tod-modal.lp",T"Device Control"},
    {"tod_wireless-modal.lp",T"Wireless Control"},
}

local tabs = {}
local session = ngx.ctx.session

for _,v in pairs(items) do
    if session:hasAccess("/modals/" .. v[1]) then
       tabs[#tabs+1] = v
    end
end

local uri = ngx.var.uri
local html = {}
for _,v in ipairs(tabs) do
    local active = ""
    local modal = "/modals/" .. v[1]
    if string.sub(uri,1,string.len(modal)) == modal then
      active = "active"
    end
    
    html[#html+1] = string.format('<li class="%s"><a id ="%s" href="#" data-remote="/modals/%s?tod">%s</a></li>', active, v[2], v[1], v[2])
end
ngx.print('<ul class="nav nav-tabs">\
');  
ngx.print(html)
ngx.print('\
</ul>\
'); 