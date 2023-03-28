--pretranslated: do not change this file
gettext.textdomain('webui-code')

local format = string.format
local ipairs = ipairs
local ngx = ngx
local session = ngx.ctx.session

local tabs = {
  {"adblck-config-modal.lp",T"Configuration"},
  {"adblck-sources-modal.lp",T"DNS Block Lists"},
  {"adblck-lists-modal.lp",T"Custom Lists"},
}

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

ngx.print('<ul class="nav nav-tabs">')
for _,v in ipairs(tabs) do
  if session:hasAccess("/modals/"..v[1]) then
    local active = ""
    if uri == ("/modals/"..v[1]) then
      active = "active"
    end
    ngx.print(format('<li class="%s"><a id="%s" href="#" class="open-modal" data-remote="/modals/%s">%s</a></li>',active,v[2],v[1],v[2]))
  end
end
ngx.print('</ul>')
