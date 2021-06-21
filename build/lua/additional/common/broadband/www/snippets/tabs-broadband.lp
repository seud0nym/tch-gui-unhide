--pretranslated: do not change this file

gettext.textdomain('webui-core')

local bridged = require("bridgedmode_helper")
if not bridged.isBridgedMode() then
  local ipairs, string, table = ipairs, string, table
  local format, len, sub = string.format, string.len, string.sub

  local items = {
    {"broadband-modal.lp", T"Configuration"},
    {"broadband-usage-modal.lp", T"Usage History"},
  }

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
end
