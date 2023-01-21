--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local format = string.format
local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")
local session = ngx.ctx.session
local modalPath = "/modals/iproutes-modal.lp"

if session:hasAccess(modalPath) then
  local bridge_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/bridge-grouping-modal.lp" data-id="bridge-grouping-modal"'
  local routes_modal_link = format('class="modal-link" data-toggle="modal" data-remote="%s" data-id="routes-modal"',modalPath)

  local function getRouteCount(path)
    local pn = proxy.getPN(path,true)
    return pn and #pn or 0
  end

  local ipv4_dynamic = getRouteCount("sys.class.iproutes.")
  local ipv4_static = getRouteCount("uci.network.route.")

  local lan_ipv6 = proxy.get("uci.network.interface.@lan.ipv6")[1].value == "1"
  local ipv6_dynamic,ipv6_static
  if lan_ipv6 then
    ipv6_dynamic = getRouteCount("sys.class.ip6routes.")
    ipv6_static = getRouteCount("uci.network.route6.")
  end

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
    ',ui_helper.createCardHeader(T"IP Routing",modalPath,nil,nil,{icon={class="icon-random"}}),'\
    <div class="content card_bg" data-bg-text="&#xf074;">\
      <span class="simple-desc">\
      <i href="#">&nbsp</i>',format(N('<strong %s>%d</strong><span %s> IPv4 Dynamic Route</span>','<strong %s>%d</strong><span %s> IPv4 Dynamic Routes</span>',ipv4_dynamic),routes_modal_link,ipv4_dynamic,routes_modal_link),'\
      </span>')
      if lan_ipv6 then
        ngx.print('\
      <span class="simple-desc">\
      <i href="#">&nbsp</i>',format(N('<strong %s>%d</strong><span %s> IPv6 Dynamic Route</span>','<strong %s>%d</strong><span %s> IPv6 Dynamic Routes</span>',ipv6_dynamic),routes_modal_link,ipv6_dynamic,routes_modal_link),'\
      </span>')
      end
      ngx.print('\
      <span class="simple-desc">\
      <i href="#">&nbsp</i>',format(N('<strong %s>%d</strong><span %s> IPv4 Static Route</span>','<strong %s>%d</strong><span %s> IPv4 Static Routes</span>',ipv4_static),routes_modal_link,ipv4_static,routes_modal_link),'\
      </span>')
      if lan_ipv6 then
        ngx.print('\
      <span class="simple-desc">\
      <i href="#">&nbsp</i>',format(N('<strong %s>%d</strong><span %s> IPv6 Static Route</span>','<strong %s>%d</strong><span %s> IPv6 Static Routes</span>',ipv6_static),routes_modal_link,ipv6_static,routes_modal_link),'\
      </span>')
      end
      ngx.print('\
      <span class="simple-desc">\
        <i href="#">&nbsp</i>',format(N('<strong %s>%d</strong><span %s> Layer 2 Bridge</span>','<strong %s>%d</strong><span %s> Layer 2 Bridges</span>',bridge_count),bridge_modal_link,bridge_count,bridge_modal_link),'\
      </span>\
    </div>\
  </div>\
</div>\
');
end
