--pretranslated: do not change this file

gettext.textdomain('webui-core')

local ipairs,string = ipairs,string
local format,len,sub = string.format,string.len,string.sub

local items = {
  {"dns-modal.lp",T"Configuration"},
  {"dns-rewrites-modal.lp",T"Rewrites"},
  {"dns-interceptd-modal.lp",T"Intercept Daemon"},
}

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
