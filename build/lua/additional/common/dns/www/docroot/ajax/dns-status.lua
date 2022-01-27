local dns_helper = require("dns_helper")
local json = require("dkjson")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")

---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local format = string.format
local tinsert = table.insert
local split = require("split").split

local dnsmasq_path = dns_helper.dnsmasq_path
local dnsmasq_enabled = "0"
local dnsmasq_status = "Custom DNS disabled"
local dns_servers = {}
local rewrites_count = 0

local noresolv = proxy.get(dnsmasq_path.."noresolv")[1].value
local disabled = proxy.get(dnsmasq_path.."disabled")
if not disabled or disabled[1].value == "0" then
  dnsmasq_enabled = "1"
  dnsmasq_status = "Custom DNS enabled"
  local addresses = proxy.getPN(dnsmasq_path.."address.",true)
  rewrites_count = #addresses
  local at = 1
  for _,dnsmsrvr in pairs(proxy.get(dnsmasq_path.."server.")) do
    local domain,ip = dns_helper.toDomainAndIP(dnsmsrvr.value)
    if not domain then
      tinsert(dns_servers,at,ip)
    else
      tinsert(dns_servers,1,format("%s&rarr;%s",domain,ip))
      at = at + 1
    end
  end
end

if #dns_servers == 0 or noresolv ~= "1" then
  if dnsmasq_enabled == "1" and #dns_servers == 0 then
    dnsmasq_enabled = "0"
    dnsmasq_status = "Custom DNS not configured"
  end
  local interface_pn = proxy.getPN("rpc.network.interface.",true)
  local dnsservers_paths = {}
  for _,v in pairs(interface_pn or {}) do
    if v.path then
      local ifconfig = proxy.get(v.path.."type",v.path.."available")
      if ifconfig[1].value == "wan" and ifconfig[2].value == "1" then
        dnsservers_paths[#dnsservers_paths + 1] = v.path.."dnsservers"
      end
    end
  end
  local dns_rpc_content = proxy.get(unpack(dnsservers_paths))
  for _,v in pairs (dns_rpc_content or {}) do
    if v and v.value ~= "" then
      for _,server in pairs(split(untaint(v.value),",")) do
        dns_servers[#dns_servers+1] = server
      end
    end
  end
end

local dns_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/dns-modal.lp" data-id="dns-modal"'
local rewrites_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/dns-rewrites-modal.lp" data-id="dns-rewrites-modal"'

local html = {}

html[#html+1] = ui_helper.createSimpleLight(dnsmasq_enabled,dnsmasq_status)
html[#html+1] = '<span class="simple-desc"><i>&nbsp</i>'
html[#html+1] = format(N('<strong %s>%d</strong><span %s> DNS Server:</span>','<strong %s>%d</strong><span %s> DNS Servers:</span>',#dns_servers),dns_modal_link,#dns_servers,dns_modal_link)
html[#html+1] = '</span><p class="subinfos" style="letter-spacing:-1px;font-size:12px;font-weight:bold;">'
local max_show = 4
for i,server in ipairs(dns_servers) do
  if i > 1 and i <= max_show then
    html[#html+1] = '<br>'
  end
  if i <= max_show then
    html[#html+1] = server
  end
end
if #dns_servers > max_show then
  html[#html+1] = T"..."
end
html[#html+1] = '</p><span class="simple-desc"><i>&nbsp</i>'
html[#html+1] = format(N('<strong%s >%d</strong><span %s> DNS Rewrite</span>','<strong %s>%d</strong><span %s> DNS Rewrites</span>',rewrites_count),rewrites_modal_link,rewrites_count,rewrites_modal_link)
html[#html+1] = '</span>'

local data = {
  html = table.concat(html,"\n"),
}

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
