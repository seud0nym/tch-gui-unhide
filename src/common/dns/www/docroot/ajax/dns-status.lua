local dns_helper = require("dns_helper")
local json = require("dkjson")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local isBridgedMode = require("bridgedmode_helper").isBridgedMode()
local format,gsub = string.format,string.gsub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local adguard = proxy.get("rpc.gui.init.files.@AdGuardHome.active")
local dnsmasq_path = dns_helper.dnsmasq_path

local html = {}

local tinsert = table.insert
local split = require("split").split

local dns_servers = {}
local rewrites_count = 0
local if_type = isBridgedMode and "lan" or "wan"

local dns_modal_link = ""
if proxy.get(dnsmasq_path.."port")[1].value ~= "0" then
  dns_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/dns-modal.lp" data-id="dns-modal"'
  local ips = {}

  local noresolv = proxy.get(dnsmasq_path.."noresolv")[1].value
  local disabled = proxy.get(dnsmasq_path.."disabled")
  if not disabled or disabled[1].value == "0" then
    local addresses = proxy.getPN(dnsmasq_path.."address.",true)
    rewrites_count = #addresses
    for _,dnsmsrvr in pairs(proxy.get(dnsmasq_path.."server.")) do
      local domain,ip = dns_helper.toDomainAndIP(dnsmsrvr.value)
      if not ips[ip] then
        ips[ip] = true
        if not domain then
          dns_servers[#dns_servers+1] = ip
        else
          tinsert(dns_servers,1,format("%s&rarr;%s",domain,ip))
        end
      end
    end
  end

  local dnsservers_paths = {}
  if #dns_servers == 0 or noresolv ~= "1" then
    local interface_pn = proxy.getPN("rpc.network.interface.",true) or {}
    for k=1,#interface_pn do
      local v = interface_pn[k]
      if v.path then
        local ifconfig = proxy.get(v.path.."type",v.path.."available",gsub(v.path,"^rpc","uci").."peerdns")
        if ifconfig[1].value == if_type and ifconfig[2].value == "1" and ifconfig[3].value ~= "0" then
          dnsservers_paths[#dnsservers_paths + 1] = v.path.."dnsservers"
        end
      end
    end
  end

  local dns_rpc_content = proxy.get(unpack(dnsservers_paths)) or {}
  for k=1,#dns_rpc_content do
    local v = dns_rpc_content[k]
    if v and v.value ~= "" then
      for _,ip in pairs(split(untaint(v.value),",")) do
        if not ips[ip] then
          ips[ip] = true
          dns_servers[#dns_servers+1] = ip
        end
      end
    end
  end
elseif adguard and adguard[1].value == "1" then
  dns_servers[#dns_servers+1] = "127.0.0.1"
end

html[#html+1] = '<span class="simple-desc"><i>&nbsp</i>'
html[#html+1] = format(N('<strong %s>%d</strong><span %s> %s DNS Server</span>','<strong %s>%d</strong><span %s> %s DNS Servers</span>',#dns_servers),dns_modal_link,#dns_servers,dns_modal_link,string.upper(if_type))
html[#html+1] = '</span><p class="subinfos" style="letter-spacing:-1px;font-size:12px;font-weight:bold;line-height:14px;margin-top:0px;margin-bottom:-3px">'

local max_show = 2
if adguard and adguard[1].value == "1" then
  local ipaddr = proxy.get("rpc.network.interface.@lan.ipaddr")[1].value
  html[#html+1] = '<style>.agh-link:visited{text-decoration:none;}</style>'
  html[#html+1] = format('-> <a class="modal-link agh-link" target="_blank" href="http://%s:8008">AdGuard Home</a>',untaint(ipaddr))
  html[#html+1] = '<br>'
  max_show = 1
else
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
end
if #dns_servers > max_show then
  html[#html+1] = "<small>+ "..(#dns_servers - max_show).." more</small>"
end

if not isBridgedMode then
  local rewrites_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/dns-modal.lp" data-id="dns-modal"'
  html[#html+1] = '</p><span class="simple-desc"><i>&nbsp</i>'
  html[#html+1] = format(N('<strong%s >%d</strong><span %s> LAN DNS Rewrite</span>','<strong %s>%d</strong><span %s> LAN DNS Rewrites</span>',rewrites_count),rewrites_modal_link,rewrites_count,rewrites_modal_link)
  html[#html+1] = '</span>'
end

if not isBridgedMode then
  local rebind_state = proxy.get(dnsmasq_path.."rebind_protection")
  if rebind_state then
    local rebind_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/dns-modal.lp" data-id="dns-modal"'
    local rebind_text
    if rebind_state[1].value ~= "0" then
      rebind_state = "1"
      rebind_text = "enabled"
    else
      rebind_state = "0"
      rebind_text = "disabled"
    end
    html[#html+1] = ui_helper.createSimpleLight(rebind_state,T(format("<span %s>LAN Rebind Protection %s</span>",rebind_modal_link,rebind_text)))
  end
  local dns_int_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/dns-hijacking-modal.lp" data-id="dns-hijacking-modal"'
  local dns4_int_state = proxy.get("uci.dns_hijacking.config.enabled")
  if dns4_int_state then
    dns4_int_state = dns4_int_state[1].value
  end
  local dns6_int_state = proxy.get("uci.tproxy.rule.@dnsv6.enabled")
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
