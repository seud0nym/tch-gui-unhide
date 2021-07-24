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
  local dns_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/dns-modal.lp" data-id="dns-modal"'
  local routes_modal_link = format('class="modal-link" data-toggle="modal" data-remote="%s" data-id="device-modal"',modalPath)

  --Dynamic Routes
  local dyn_count = proxy.getPN("sys.class.iproutes.",true)
  dyn_count = dyn_count and #dyn_count or 0

  --IPv4 Static Routes
  local ipv4_count = proxy.getPN("uci.network.route.",true)
  ipv4_count = ipv4_count and #ipv4_count or 0

  --DNS
  local function split(x,sep)
    x = tostring(x)
    local sep,fields = sep or ":",{}
    local pattern = format("([^%s]+)",sep)
    x:gsub(pattern,function(c) fields[#fields+1] = c end)
    return fields
  end
  local v
  local dns_rpc_path = "rpc.network.interface."
  local interface_pn = proxy.getPN(dns_rpc_path,true)
  local dnsservers_paths = {}
  for _,v in pairs(interface_pn or {}) do
    if v.path then
      dnsservers_paths[#dnsservers_paths + 1] = v.path .. "dnsservers"
    end
  end
  local dns_rpc_content = proxy.get(unpack(dnsservers_paths)) -- use unpack for get() compatibility
  local dns_count = 0
  for _,v in pairs (dns_rpc_content or {}) do
    if v and v.value ~= "" then
      local subcount = split(format("%s",v.value),",")
      dns_count = dns_count + #subcount
    end
  end

  local custom_dns_count = 0
  local dnsmidx, dnsmif
  for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.", true)) do
    for _,dnsmif in pairs(proxy.get(dnsmidx.path.."interface.")) do
      if dnsmif.value == "lan" then
        local addresses = proxy.getPN(dnsmidx.path .. "address.", true)
        custom_dns_count = #addresses
        break
      end
    end
  end

  local bridge_count = 0
  for _,v in pairs(proxy.getPN("uci.network.interface.", true)) do
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
  		<div class="divtable">\
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
        <span class="simple-desc">\
          <i href="#">&nbsp</i>\
          ');
          ngx.print(format(N('<strong %s>%d</strong><span %s> DNS Server</span>','<strong %s>%d</strong><span %s> DNS Servers</span>',dns_count),dns_modal_link,dns_count,dns_modal_link))
          ngx.print('\
        </span>\
        <span class="simple-desc">\
          <i href="#">&nbsp</i>\
          ');
          ngx.print(format(N('<strong %s>%d</strong><span %s> Custom DNS Entry</span>','<strong %s>%d</strong><span %s> Custom DNS Entries</span>',custom_dns_count),dns_modal_link,custom_dns_count,dns_modal_link))
          ngx.print('\
        </span>\
        <span class="simple-desc">\
          <i href="#">&nbsp</i>\
          ');
          ngx.print(format(N('<strong %s>%d</strong><span %s> Layer 2 Bridge</span>','<strong %s>%d</strong><span %s> Layer 2 Bridges</span>',bridge_count),bridge_modal_link,bridge_count,bridge_modal_link))
          ngx.print('\
        </span>\
      </div>\
    </div>\
  </div>\
</div>\
');
end
ngx.print('\
');
