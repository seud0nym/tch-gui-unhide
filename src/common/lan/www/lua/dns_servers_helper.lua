local proxy = require("datamodel")
local string = string
local format,gsub = string.format,string.gsub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

local ipaddr = proxy.get("uci.network.interface.@lan.ipaddr")
local name = proxy.get("uci.env.var.variant_friendly_name")
local variant
if not name then
  name = proxy.get("env.var.prod_friendly_name")
  variant = gsub(untaint(name[1].value),"Technicolor ","")
else
  variant = gsub(untaint(name[1].value),"TLS","")
end
local lan_ip = untaint(ipaddr[1].value)

local dns_servers = {
  { -- IPv4 DNS servers
    {"",T""},
    {"custom",T"Enter Custom IP Address"},
    {lan_ip,T(format("%s (%s)",variant,lan_ip))},
    -- insert ipv4-DNS-Servers file content here (Do NOT change or remove this comment - used by 105-DNS)
    {"8.8.8.8",T"Google (8.8.8.8)"},
    {"8.8.4.4",T"Google (8.8.4.4)"},
    {"1.1.1.1",T"Cloudflare (1.1.1.1)"},
    {"1.0.0.1",T"Cloudflare (1.0.0.1)"},
    {"1.1.1.2",T"Cloudflare (1.1.1.2)"},
    {"1.0.0.2",T"Cloudflare (1.0.0.2)"},
    {"1.1.1.3",T"Cloudflare (1.1.1.3)"},
    {"1.0.0.3",T"Cloudflare (1.0.0.3)"},
    {"208.67.222.222",T"OpenDNS (208.67.222.222)"},
    {"208.67.220.220",T"OpenDNS (208.67.220.220)"},
    {"208.67.222.123",T"OpenDNS FamilyShield (208.67.222.123)"},
    {"208.67.220.123",T"OpenDNS FamilyShield (208.67.220.123)"},
    {"9.9.9.9",T"Quad9 (9.9.9.9)"},
    {"149.112.112.112",T"Quad9 (149.112.112.112)"},
    {"94.140.14.14", T"AdGuard DNS (94.140.14.14)"},
    {"94.140.15.15", T"AdGuard DNS (94.140.15.15)"},
    {"94.140.14.15", T"AdGuard Family Protection DNS (94.140.14.15)"},
    {"94.140.15.16", T"AdGuard Family Protection DNS (94.140.15.16)"},
    {"94.140.14.140", T"AdGuard Non-filtering DNS (94.140.14.140)"},
    {"94.140.14.141", T"AdGuard Non-filtering DNS (94.140.14.141)"},
    {"64.6.64.6",T"Verisign (64.6.64.6)"},
    {"64.6.65.6",T"Verisign (64.6.65.6)"},
    {"8.26.56.26",T"Comodo (8.26.56.26)"},
    {"8.20.247.20",T"Comodo (8.20.247.20)"},
    {"81.218.119.11",T"GreenTeam (81.218.119.11)"},
    {"209.88.198.133",T"GreenTeam (209.88.198.133)"},
    {"195.46.39.39",T"SafeDNS (195.46.39.39)"},
    {"195.46.39.40",T"SafeDNS (195.46.39.40)"},
    {"216.146.35.35",T"Dyn (216.146.35.35)"},
    {"216.146.36.36",T"Dyn (216.146.36.36)"},
    {"198.101.242.72",T"Alternate DNS (198.101.242.72)"},
    {"23.253.163.53",T"Alternate DNS (23.253.163.53)"},
    {"77.88.8.8",T"Yandex.DNS (77.88.8.8)"},
    {"77.88.8.1",T"Yandex.DNS (77.88.8.1)"},
    {"91.239.100.100",T"UncensoredDNS (91.239.100.100)"},
    {"89.233.43.71",T"UncensoredDNS (89.233.43.71)"},
    {"156.154.70.1",T"Neustar (156.154.70.1)"},
    {"156.154.71.1",T"Neustar (156.154.71.1)"},
    {"45.77.165.194",T"Fourth Estate (45.77.165.194)"},
    {"45.32.36.36",T"Fourth Estate (45.32.36.36)"},
    {"54.252.183.4",T"GetFlix (54.252.183.4)"},
    {"54.252.183.5",T"GetFlix (54.252.183.5)"},
    {"185.228.168.168", T"CleanBrowsing Family (185.228.168.168)"},
    {"185.228.169.168", T"CleanBrowsing Family (185.228.169.168)"},
    {"185.228.168.10", T"CleanBrowsing Adult (185.228.168.10)"},
    {"185.228.168.11", T"CleanBrowsing Adult (185.228.168.11)"},
    {"185.228.168.9", T"CleanBrowsing Security (185.228.168.9)"},
    {"185.228.169.9", T"CleanBrowsing Security (185.228.169.9)"},
  },
  { -- IPv6 DNS servers
    {"",T""},
    {"custom",T"Enter Custom IP Address"},
    -- insert ipv6-DNS-Servers file content here (Do NOT change or remove this comment - used by 105-DNS)
    {"2001-4860-4860--8888",T"Google (2001:4860:4860::8888)"},
    {"2001-4860-4860--8844",T"Google (2001:4860:4860::8844)"},
    {"2606-4700-4700--1111",T"Cloudflare (2606:4700:4700::1111)"},
    {"2606-4700-4700--1001",T"Cloudflare (2606:4700:4700::1001)"},
    {"2606-4700-4700--1112",T"Cloudflare (2606:4700:4700::1112)"},
    {"2606-4700-4700--1002",T"Cloudflare (2606:4700:4700::1002)"},
    {"2606-4700-4700--1113",T"Cloudflare (2606:4700:4700::1113)"},
    {"2606-4700-4700--1003",T"Cloudflare (2606:4700:4700::1003)"},
    {"2620-119-35--35",T"OpenDNS (2620:119:35::35)"},
    {"2620-119-53--53",T"OpenDNS (2620:119:53::53)"},
    {"2a10-50c0--ad1-ff", T"AdGuard DNS (2a10:50c0::ad1:ff)"},
    {"2a10-50c0--ad2-ff", T"AdGuard DNS (2a10:50c0::ad2:ff)"},
    {"2a10-50c0--bad1-ff", T"AdGuard Family Protection DNS (2a10:50c0::bad1:ff)"},
    {"2a10-50c0--bad2-ff", T"AdGuard Family Protection DNS (2a10:50c0::bad2:ff)"},
    {"2a10-50c0--1-ff", T"AdGuard Non-filtering DNS (2a10:50c0::1:ff)"},
    {"2a10-50c0--2-ff", T"AdGuard Non-filtering DNS (2a10:50c0::2:ff)"},
    {"2620-74-1b--1-1",T"Verisign (2620:74:1b::1:1)"},
    {"2620-74-1c--2-2",T"Verisign (2620:74:1c::2:2)"},
    {"2a02-6b8--feed-0ff",T"Yandex.DNS (2a02:6b8::feed:0ff)"},
    {"2a02-6b8-0-1--feed-0ff",T"Yandex.DNS (2a02:6b8:0:1::feed:0ff)"},
    {"2001-67c-28a4--",T"UncensoredDNS (2001:67c:28a4::)"},
    {"2a01-3a0-53-53--",T"UncensoredDNS (2a01:3a0:53:53::)"},
    {"2610-a1-1018--1",T"Neustar (2610:a1:1018::1)"},
    {"2610-a1-1019--1",T"Neustar (2610:a1:1019::1)"},
    {"2a0d-2a00-1--", T"CleanBrowsing Family (2a0d:2a00:1::)"},
    {"2a0d-2a00-2--", T"CleanBrowsing Family (2a0d:2a00:2::)"},
    {"2a0d-2a00-1--1", T"CleanBrowsing Adult (2a0d:2a00:1::1)"},
    {"2a0d-2a00-2--1", T"CleanBrowsing Adult (2a0d:2a00:2::1)"},
    {"2a0d-2a00-1--2", T"CleanBrowsing Security (2a0d:2a00:1::2)"},
    {"2a0d-2a00-2--2", T"CleanBrowsing Security (2a0d:2a00:2::2)"},
  }
}

-- ethernet-modal.lp can dynamically add items, so return a copy
function M.all()
  local copy = { {}, {} }
  for i=1,2,1 do
    for k=1,#dns_servers[i] do
      copy[i][#copy[i]+1]=dns_servers[i][k]
    end
  end
  return copy
end

-- Doesn't change, so just return table directly
M.external = { {}, {} }
for i=1,2,1 do
  for k=1,#dns_servers[i] do
    local addr = dns_servers[i][k][1]
    if addr ~= "" and addr ~= "custom" and addr ~= lan_ip then
      M.external[i][#M.external[i]+1]=dns_servers[i][k]
    end
  end
end

return M