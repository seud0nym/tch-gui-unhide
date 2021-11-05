--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local format = string.format
local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")
local session = ngx.ctx.session
local modalPath = "/modals/iproutes-modal.lp"

if session:hasAccess(modalPath) then
  local bridge_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/bridge-grouping-modal.lp" data-id="dns-modal"'
  local dns_modal_link
  local routes_modal_link = format('class="modal-link" data-toggle="modal" data-remote="%s" data-id="device-modal"',modalPath)

  local dyn_count = proxy.getPN("sys.class.iproutes.",true)
  dyn_count = dyn_count and #dyn_count or 0

  local ipv4_count = proxy.getPN("uci.network.route.",true)
  ipv4_count = ipv4_count and #ipv4_count or 0

  local bridge_count = 0
  for _,v in pairs(proxy.getPN("uci.network.interface.",true)) do
    local type = proxy.get(v.path.."type")
    if type and type[1].value == "bridge" then
      bridge_count = bridge_count + 1
    end
  end

  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeader(T"IP Routing","/modals/iproutes-modal.lp") ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf018;">\
      <span class="simple-desc">\
        <i href="#">&nbsp</i>\
        ');
        ngx.print(format(N('<strong %s>%d</strong><span %s> IPv4 Route</span>','<strong %s>%d</strong><span %s> IPv4 Routes</span>',dyn_count),routes_modal_link,dyn_count,routes_modal_link))
        ngx.print('\
      </span>\
      <span class="simple-desc">\
        <i href="#">&nbsp</i>\
        ');
        ngx.print(format(N('<strong %s>%d</strong><span %s> IPv4 Static Route</span>','<strong %s>%d</strong><span %s> IPv4 Static Routes</span>',ipv4_count),routes_modal_link,ipv4_count,routes_modal_link))
        ngx.print('\
      </span>\
      ');
      ngx.print('\
      <span class="simple-desc">\
        <i href="#">&nbsp</i>\
        ');
        ngx.print(format(N('<strong %s>%d</strong><span %s> Layer 2 Bridge</span>','<strong %s>%d</strong><span %s> Layer 2 Bridges</span>',bridge_count),bridge_modal_link,bridge_count,bridge_modal_link))
        ngx.print('\
      </span>\
    </div>\
  </div>\
</div>\
');
end
ngx.print('\
');
