local common = require("common_helper")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")

---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local find,format,gmatch,gsub,lower,match = string.find,string.format,string.gmatch,string.gsub,string.lower,string.match


local M = {}

function M.getWireguardCardHTML()
  local content = {
    server = "uci.wireguard.@wg0.enabled",
    firewall = "uci.firewall.rule.@wg0.enabled",
    interface_count = "rpc.gui.wireguard.interface_count",
    server_peers = "rpc.gui.wireguard.server_peers",
    server_active_peers = "rpc.gui.wireguard.server_active_peers",
    server_rx_bytes = "rpc.gui.wireguard.server_rx_bytes",
    server_tx_bytes = "rpc.gui.wireguard.server_tx_bytes",
    server_interface_count = "rpc.gui.wireguard.server_interface_count",
    server_interfaces = "rpc.gui.wireguard.server_interfaces",
    client_peers = "rpc.gui.wireguard.client_peers",
    client_active_peers = "rpc.gui.wireguard.client_active_peers",
    client_rx_bytes = "rpc.gui.wireguard.client_rx_bytes",
    client_tx_bytes = "rpc.gui.wireguard.client_tx_bytes",
    client_interface_count = "rpc.gui.wireguard.client_interface_count",
    client_interfaces = "rpc.gui.wireguard.client_interfaces",
    external_ipv4_address = "rpc.gui.wireguard.external_ipv4_address",
    external_ipv6_address = "rpc.gui.wireguard.external_ipv6_address",
  }
  content_helper.getExactContent(content)

  if content.server ~= "0" then
    content.server = "1"
    content.server_text = T"VPN Server enabled"
  else
    content.server_text = T"VPN Server disabled"
  end
  if content.firewall ~= "0" then
    content.firewall = "1"
    content.firewall_text = T"Incoming Firewall Port open"
  else
    content.firewall_text = T"Incoming Firewall Port closed"
    if content.server == "1" then
      content.server = "2"
    end
  end

  local interfaces = proxy.getPN("uci.wireguard.",true)

  local transfer_pattern = '<i class="icon-cloud-upload status-icon"></i> <span style="width:5em;display:inline-block;">%s</span>&ensp;<i class="icon-cloud-download status-icon"></i> <span style="width:5em;display:inline-block;">%s</span><br>'

  local html = {}
  html[#html+1] = ui_helper.createSimpleLight(content.server,content.server_text)
  if content.server == "2" then
    html[#html+1] = ui_helper.createSimpleLight(content.firewall,content.firewall_text)
  elseif content.server == "1" then
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format("%s of %s server peers active<br>",content.server_active_peers,content.server_peers)
    html[#html+1] = format(transfer_pattern,common.bytes2string(content.server_tx_bytes),common.bytes2string(content.server_rx_bytes))
    html[#html+1] = '</p>'
  end
  if #interfaces > 1 then
    if content.client_active_peers == "0" then
      html[#html+1] = ui_helper.createSimpleLight("0",T"VPN Client inactive")
    else
      html[#html+1] = ui_helper.createSimpleLight("1",T(format("VPN Client %s active",content.client_interfaces)))
      html[#html+1] = '<p class="subinfos">'
      html[#html+1] = format(transfer_pattern,common.bytes2string(content.client_tx_bytes),common.bytes2string(content.client_rx_bytes))
      if content.external_ipv4_address or content.external_ipv6_address then
        html[#html+1] = format(T'External IP: <strong style="letter-spacing:-1px"><span style="font-size:12px">%s</span></strong>',content.external_ipv4_address)
        html[#html+1] = format(T'<br><strong style="letter-spacing:-1px"><span style="font-size:12px">%s</span></strong>',content.external_ipv6_address)
      end
      html[#html+1] = '</p>'
    end
  elseif #interfaces == 1 then
    html[#html+1] = ui_helper.createSimpleLight("0",T"No VPN Client Interfaces defined")
  elseif tonumber(content.client_interface_count) > 1 then
    html[#html+1] = ui_helper.createSimpleLight("1",T(format('%s of %s VPN Clients active',content.client_active_peers,content.client_interface_count)))
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format(transfer_pattern,common.bytes2string(content.client_tx_bytes),common.bytes2string(content.client_rx_bytes))
    html[#html+1] = '</p>'
  end

  return html
end

function M.genkey()
  local key
  local cmd = io.popen('/usr/bin/wg-go genkey','r')
  if cmd then
    key = cmd:read("*line")
    cmd:close()
    return key
  end
  return nil,"Failed to generate private key"
end

function M.genpsk()
  local key
  local cmd = io.popen('/usr/bin/wg-go genpsk','r')
  if cmd then
    key = cmd:read("*line")
    cmd:close()
    return key
  end
  return nil,"Failed to generate pre-shared key"
end

function M.pubkey(private_key)
  if private_key and private_key ~= "" then
    local key
    local cmd = io.popen(format('echo "%s" | /usr/bin/wg-go pubkey',untaint(private_key)),'r')
    if cmd then
      key = cmd:read("*line")
      cmd:close()
      return key
    end
    return nil,"Failed to generate public key"
  end
  return nil,"No private key specified to return public key"
end

local ipv4pattern = "(%d+%.%d+%.%d+%.)(%d+)/(%d+)"
local ipv6pattern = "([:%x]+:)(%x+)/(%d+)"

function M.getUnusedIP()
  local ip4_prefix,ip4_suffix,ip4_netmask,ip4_next
  local ip6_prefix,ip6_netmask,ip6_next
  local used

  for _,ip in pairs(proxy.get("uci.wireguard.@wg0.address.")) do
    local ipaddr = untaint(ip.value)
    if not ip4_prefix then
      ip4_prefix,ip4_suffix,ip4_netmask = match(ipaddr,ipv4pattern)
      used = { tonumber(ip4_suffix) }
    end
    if not ip6_prefix then
      ip6_prefix,_,ip6_netmask = match(ipaddr,ipv6pattern)
    end
  end

  if not used then
    return nil,"No IPv4 address found for interface wg0?"
  end

  for _,peer in pairs(proxy.getPN("uci.wireguard.@wg0.peer.",true)) do
    for _,ip in pairs(proxy.get(peer.path.."allowed_ip.")) do
      local _,octet = match(untaint(ip.value),ipv4pattern)
      if octet then
        used[#used + 1] = octet
      end
    end
  end
  for suffix = 2,255 do
    local s = tostring(suffix)
    local found
    for index = 1,#used do
      if used[index] == s then
        found = true
        break
      end
    end
    if not found then
      ip4_next = ip4_prefix..s
      if ip6_prefix then
        ip6_next = format("%s%x",ip6_prefix,suffix)
      end
      break
    end
  end

  if not ip4_next then
    return nil,"No unused IPs found?"
  end

  return ip4_next,ip4_netmask,ip6_next,ip6_netmask
end

function M.getDNS(include_ipv6)
  local dns = ""

  local adguard = proxy.get("rpc.gui.init.files.@AdGuardHome.active")
  if not adguard or adguard[1].value ~= "1" then
    for _,v in pairs(proxy.getPN("uci.dhcp.dhcp.@lan.dhcp_option.",true)) do
      local ipv4_dns = match(untaint(proxy.get(v.path.."value")[1].value),"6,(.+)")
      if ipv4_dns then
        if dns == "" then
          dns = ipv4_dns
        else
          dns = format("%s,%s",dns,ipv4_dns)
        end
      end
    end
    if include_ipv6 == true then
      for _,v in pairs(proxy.getPN("uci.dhcp.dhcp.@lan.dns.",true)) do
        local ipv6_dns = untaint(proxy.get(v.path.."value")[1].value)
        if dns == "" then
          dns = ipv6_dns
        else
          dns = format("%s,%s",dns,ipv6_dns)
        end
      end
    end
  end

  if dns == "" then
    local addresses = proxy.get("rpc.network.interface.@lan.ipaddr","rpc.network.interface.@lan.ip6addr")
    if addresses and addresses[1] then
      if include_ipv6 and addresses[2] and addresses[2].value ~= "" then
        dns = format("%s,%s",addresses[1].value,gsub(untaint(addresses[2].value),"%s+",","))
      else
        dns = untaint(addresses[1].value)
      end
    end
  end

  return dns
end

function M.receiveFile(filename,match)
  local function do_receive(outfile,match)
    local upload = require("web.fileupload")
    local form,err = upload.fromstream()
    if not form then
      return false,1,"failed to create upload ctx: "..err
    end
    local totalsize = 0
    local file
    local discard = false
    while true do
      local token,data,err = form:read()
      if not token then
        return false,2,"read failed: "..err
      end
      if token == "header" then
        if not discard and not file and find(data[2],match,1,true) then
          file = outfile
        end
        if not discard and not file then
          return false,3,"failed to start receiving file"
        end
      elseif token == "body" then
        if file then
          totalsize = totalsize + #data
          file:write(data)
        end
      elseif token == "part_end" then
        if file then
          file = nil
          discard = true
        end
      elseif token == "eof" then
        break
      end
    end
    return true
  end

  local file = io.open(filename,"w")
  local result,err_code,err_msg
  if file then
    result,err_code,err_msg = do_receive(file,match)
  else
    file = io.open("/dev/null","w")
    do_receive(file,match)
    result = false
    err_code = 4
    err_msg = "internal error"
  end

  file:close()

  return result,err_code,err_msg
end

local option_convert = {
  address = "addresses",
  allowedips = "allowed_ips",
  endpoint = "endpoint",
  fwmark = "fwmark",
  listenport = "listen_port",
  persistentkeepalive = "persistent_keepalive",
  privatekey = "private_key",
  presharedkey = "preshared_key",
  publickey = "public_key",
  mtu = "mtu",
}

function M.parseConfig(content)
  local interface_config,peer_config = {},{}
  local in_interface,in_peer = false,false
  for line in gmatch(untaint(content)..'\n',"([^\r\n]*)[\r\n]") do
    if not match(line,"^%s*$") then
      local key,value = match(line,"^(%S+)%s*=%s*(.+)$")
      if key then
        local option = option_convert[lower(key)]
        if option and value ~= "" then
          if in_interface then
            interface_config[option] = value
          elseif in_peer then
            if option == "endpoint" then
              local host,port = match(value,"(%S+):(%d+)")
              if not port then
                host = value
                port = "51820"
              end
              peer_config[#peer_config]["endpoint_host"] = host
              peer_config[#peer_config]["endpoint_port"] = port
            else
              peer_config[#peer_config][option] = value
            end
          end
        end
      elseif not in_interface then
        if match(line,"^%[Interface%]$") then
          in_interface = true
        end
      elseif match(line,"^%[Peer%]$") then
        in_interface = false
        in_peer = true
        peer_config[#peer_config + 1] = {}
      end
    end
  end

  return interface_config,peer_config
end

local function getFirewallWANZoneNetworkPath()
  local zones = proxy.getPN("uci.firewall.zone.",true)
  for _,v in ipairs(zones) do
    local wan = proxy.get(v.path.."wan")
    if wan and wan[1].value == "1" then
      return v.path.."network."
    end
  end
  return nil
end

local function getInterfaceFirewallWANZoneNetworkPath(ifname)
  local path = getFirewallWANZoneNetworkPath()
  for _,network in pairs(proxy.getPN(path,true)) do
    local value = proxy.get(network.path.."value")[1].value
    if value == ifname then
      return network.path,path
    end
  end
  return nil,path
end

function M.deleteInterface(ifname)
  for _,peer in pairs(proxy.getPN("uci.wireguard.@"..ifname..".peer.",true)) do
    local okay,errmsg,errcode = proxy.del(peer.path)
    if not okay then
      return nil,errmsg,errcode
    end
  end
  local path = getInterfaceFirewallWANZoneNetworkPath(ifname)
  if path then
    proxy.del(path)
  end
  return proxy.del("uci.wireguard.@"..ifname..".")
end

function M.addInterfaceToFirewallWANZone(ifname)
  local path,parent = getInterfaceFirewallWANZoneNetworkPath(ifname)
  if not path then
    local index = proxy.add(parent)
    return proxy.set(parent.."@"..index..".value",ifname)
  end
  return nil, "Interface '"..ifname.."' already in WAN firewall zone"
end

return M
