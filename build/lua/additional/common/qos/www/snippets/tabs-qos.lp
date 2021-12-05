--pretranslated: do not change this file

gettext.textdomain('webui-core')

local items = {
  {"qos-bqos-modal.lp",T"Bandwidth Shaping"},
  {"qos-iqos-modal.lp",T"Ingress QoS"},
  {"qos-device-modal.lp",T"Egress QoS"},
  {"qos-label-modal.lp",T"Labels"},
  {"qos-classify-modal.lp",T"Classify Rules"},
  {"qos-reclassify-modal.lp",T"Reclassify Rules"},
  {"qos-queue-modal.lp",T"Queues"},
  {"qos-swshaper-modal.lp",T"Traffic Shaping"},
}

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

local html = {}

html[#html+1] = '<ul class="nav nav-tabs">'
for _,v in ipairs(items) do
  local active = ""
  local modal = "/modals/"..v[1]
  if string.sub(uri,1,string.len(modal)) == modal then
    active = "active"
  end
  html[#html+1] = string.format('<li class="%s"><a href="#" data-remote="/modals/%s">%s</a></li>',active,v[1],v[2])
end
html[#html+1] = '</ul>'

ngx.print(html)

