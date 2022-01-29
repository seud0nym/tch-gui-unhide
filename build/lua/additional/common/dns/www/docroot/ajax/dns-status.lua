local dns_helper = require("dns_helper")
local json = require("dkjson")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")

---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local format = string.format
local tinsert = table.insert
local split = require("split").split
local isBridgedMode = require("bridgedmode_helper").isBridgedMode()

local dnsmasq_path = dns_helper.dnsmasq_path
local dns_servers = {}
local rewrites_count = 0

local noresolv = proxy.get(dnsmasq_path.."noresolv")[1].value
local disabled = proxy.get(dnsmasq_path.."disabled")
if not disabled or disabled[1].value == "0" then
  local addresses = proxy.getPN(dnsmasq_path.."address.",true)
  rewrites_count = #addresses
  for _,dnsmsrvr in pairs(proxy.get(dnsmasq_path.."server.")) do
    local domain,ip = dns_helper.toDomainAndIP(dnsmsrvr.value)
    if not domain then
      dns_servers[#dns_servers+1] = ip
    else
      tinsert(dns_servers,1,format("%s&rarr;%s",domain,ip))
    end
  end
end

local dnsservers_paths = {}
if isBridgedMode then
  dnsservers_paths[#dnsservers_paths+1] = "rpc.network.interface.@lan.dnsservers"
else
  if #dns_servers == 0 or noresolv ~= "1" then
    local interface_pn = proxy.getPN("rpc.network.interface.",true)
    for _,v in pairs(interface_pn or {}) do
      if v.path then
        local ifconfig = proxy.get(v.path.."type",v.path.."available")
        if ifconfig[1].value == "wan" and ifconfig[2].value == "1" then
          dnsservers_paths[#dnsservers_paths + 1] = v.path.."dnsservers"
        end
      end
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

local dns_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/dns-modal.lp" data-id="dns-modal"'

local html = {}

html[#html+1] = '<span class="simple-desc"><i>&nbsp</i>'
html[#html+1] = format(N('<strong %s>%d</strong><span %s> DNS Server</span>','<strong %s>%d</strong><span %s> DNS Servers</span>',#dns_servers),dns_modal_link,#dns_servers,dns_modal_link)
html[#html+1] = '</span><p class="subinfos" style="letter-spacing:-1px;font-size:12px;font-weight:bold;line-height:15px;">'
local max_show = 3
if isBridgedMode then
  max_show = 4
end
for i,server in ipairs(dns_servers) do
  if i > 1 and i <= max_show then
    html[#html+1] = '<br>'
  end
  if i <= max_show then
    html[#html+1] = server
  end
end
if #dns_servers > max_show then
  html[#html+1] = "<small>+".." "..(#dns_servers - max_show).." more</small>"
end

if not isBridgedMode then
  local rewrites_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/dns-rewrites-modal.lp" data-id="dns-rewrites-modal"'
  html[#html+1] = '</p><span class="simple-desc"><i>&nbsp</i>'
  html[#html+1] = format(N('<strong%s >%d</strong><span %s> DNS Rewrite</span>','<strong %s>%d</strong><span %s> DNS Rewrites</span>',rewrites_count),rewrites_modal_link,rewrites_count,rewrites_modal_link)
  html[#html+1] = '</span>'
end

local interceptd = proxy.get("uci.intercept.config.enabled","uci.intercept.dns.spoofip")
local interceptd_enabled
local interceptd_status
if interceptd and interceptd[1].value == "0" then
  interceptd_enabled = "0"
  interceptd_status = "disabled"
else
  interceptd_enabled = "1"
  interceptd_status = "enabled"
end
html[#html+1] = ui_helper.createSimpleLight(interceptd_enabled,T(format('<span class="modal-link" data-toggle="modal" data-remote="/modals/dns-interceptd-modal.lp" data-id="dns-interceptd-modal">Intercept Daemon %s</span>',interceptd_status)))

if not isBridgedMode then
  local dns6_int_state = proxy.get("uci.tproxy.rule.@dnsv6.enabled")
  local dns4_int_state = "0"
  local dns_int_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/firewall-dns_int-modal.lp" data-id="firewall-dns_int-modal"'
  for _,v in ipairs(proxy.getPN("uci.firewall.redirect.",true)) do
    local path = v.path
    local values = proxy.get(path.."name",path.."enabled")
    if values then
      local name = values[1].value
      if name == "Redirect-DNS" or name == "Intercept-DNS" then
        if values[2] and values[2].value ~= "0" then
          dns4_int_state = "1"
        end
        break
      end
    end
  end
  if dns6_int_state then
    dns6_int_state = dns6_int_state[1].value
  end
  local dns_int_state, dns_int_text, dns_int_family
  if dns4_int_state == "1" or dns6_int_state == "1" then
    dns_int_state = "1"
    dns_int_text = "enabled"
    if dns4_int_state == "1" and dns6_int_state == "1" then
      dns_int_family = ""
    elseif dns4_int_state == "1" then
      dns_int_family = "IPv4"
    else
      dns_int_family = "IPv6"
    end
  else
    dns_int_state = "0"
    dns_int_text = "disabled"
    dns_int_family = ""
  end
  html[#html+1] = ui_helper.createSimpleLight(dns_int_state,T(format("<span %s>%s DNS Hijacking %s</span>",dns_int_modal_link,dns_int_family,dns_int_text)))
end

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
