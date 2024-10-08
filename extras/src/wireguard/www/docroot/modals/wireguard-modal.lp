--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-code')

local common = require("common_helper")
local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local wireguard_helper = require("wireguard_helper")
local proxy = require("datamodel")
local proxy_helper = require("proxy_helper")

---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local find,format,gsub,match,sub = string.find,string.format,string.gsub,string.match,string.sub

local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString
local vSIP = post_helper.validateStringIsPort
local vUN = post_helper.validateUciName

local getargs = ngx.req.get_uri_args()
local getintf = untaint(getargs.intf)
local deleting = false

local function importInterface(new_if_name,content)
  local name_valid,name_invalid = vUN(new_if_name)
  if not name_valid then
    message_helper.pushMessage(T(format("Failed to import %s: Name contains invalid characters (%s)",new_if_name,name_invalid)),"error")
  else
    if proxy.get(format("uci.wireguard.@%s.enabled",new_if_name)) then
      message_helper.pushMessage(T(format("Failed to import %s: Interface already exists",new_if_name)),"error")
    else
      local interface_config,peer_config = wireguard_helper.parseConfig(content)
      if not interface_config.private_key then
        message_helper.pushMessage(T(format("Failed to import %s: [Interface] section or PrivateKey not found?",new_if_name)),"error")
      elseif interface_config.listen_port and interface_config.listen_port ~= "" then
        message_helper.pushMessage(T(format("Failed to import %s: Multiple servers not supported at this time.",new_if_name)),"error")
      elseif #peer_config == 0 or not next(peer_config[1]) then
        message_helper.pushMessage(T(format("Failed to import %s: [Peer] section not found or empty?",new_if_name)),"error")
      else
        local iface,errmsg = proxy.add("uci.wireguard.",new_if_name)
        if errmsg then
          message_helper.pushMessage(T(string.format("Failed to add interface %s: %s",new_if_name,errmsg)),"error")
        else
          local commit = true
          for k,v in pairs(interface_config) do
            if not proxy_helper.set(format("uci.wireguard.@%s.%s",iface,k),v) then
              commit = false
              break
            end
          end
          if commit then
            for i,t in pairs(peer_config) do
              local peer_name
              if #peer_config == 1 then
                peer_name = format("%s_peer",iface)
              else
                peer_name = format("%s_peer%02d",iface,i)
              end
              local index,errmsg = proxy.add("uci.wireguard.@"..iface..".peer.",peer_name)
              if errmsg then
                message_helper.pushMessage(T(string.format("Failed to add peer %s to interface %s: %s",peer_name,iface,errmsg)),"error")
                commit = false
                break
              else
                for k,v in pairs(t) do
                  if not proxy_helper.set(format("uci.wireguard.@%s.peer.@%s.%s",iface,index,k),v) then
                    commit = false
                    break
                  end
                end
                if commit then
                  if not proxy_helper.set(format("uci.wireguard.@%s.peer.@%s.route_allowed_ips",iface,index),"1") then
                    commit = false
                  end
                end
              end
            end
            if commit then
              local result,errmsg = wireguard_helper.addInterfaceToFirewallWANZone(iface)
              if not result then
                ngx.log(ngx.ERR,errmsg)
              end
              proxy.apply()
            else
              proxy.del(format("uci.wireguard.%s",new_if_name))
              local okay,errmsg,errcode = wireguard_helper.deleteInterface(new_if_name)
              if not okay then
                message_helper.pushMessage(T(string.format("Failed to delete interface '%s': %s (%s)",new_if_name,errmsg,errcode)),"error")
              end
            end
          end
        end
      end
    end
  end
end

local function deleteInterface(content)
  if type(content) == "table" then
    for _,checked in pairs(content) do
      if checked == "_TRUE_" then
        local okay,errmsg,errcode = wireguard_helper.deleteInterface(getintf)
        if not okay then
          message_helper.pushMessage(T(string.format("Failed to delete interface '%s': %s (%s)",getintf,errmsg,errcode)),"error")
        else
          proxy.apply()
          getintf = "wg0"
          deleting = true
          break
        end
      end
    end
  end
end

local function checkForMultipleEnabledClients()
  local state = proxy.get("uci.wireguard.@"..getintf..".enabled")
  if state and state[1].value == "0" then
    local commit = false
    for _,intf in pairs(proxy.getPN("uci.wireguard.",true)) do
      if intf.path ~= "uci.wireguard.@wg0." and intf.path ~= "uci.wireguard.@"..getintf.."." then
        local path = intf.path.."enabled"
        if proxy.get(path)[1].value == "1" then
          if proxy_helper.set(path,"0") then
            commit = true
          else
            break
          end
        end
      end
    end
    if commit then
      proxy.apply()
    end
  end
end

local function updateFirewall(args)
  local fw = proxy.get("uci.firewall.rule.@wg0.enabled","uci.firewall.rule.@wg0.dest_port","uci.firewall.rule.@wg0.family")
  if fw then
    local applyfw = false
    if fw[1] and args.enabled and args.enabled ~= fw[1].value then
      if proxy_helper.set("uci.firewall.rule.@wg0.enabled",untaint(args.enabled)) then
        applyfw = true
      end
    end
    if fw[2] and args.listen_port and args.listen_port ~= fw[2].value then
      if proxy_helper.set("uci.firewall.rule.@wg0.dest_port",untaint(args.listen_port)) then
        applyfw = true
      end
    end
    if fw[3] and args.ipv6 then
      local ipv6 = args.ipv6
      local family
      if ipv6 == "1" and family ~= "" then
        family = ""
      elseif ipv6 == "0" and family ~= "ipv4" then
        family = "ipv4"
      end
      if family and proxy_helper.set("uci.firewall.rule.@wg0.family",family) then
        applyfw = true
      end
    end
    if applyfw then
      proxy.apply()
    end
  end
end

if ngx.var.request_method == "POST" then
  local args = ngx.req.get_post_args()
  if args.action == "SAVE" then
    for arg,content in pairs(args) do
      local new_if_name = match(arg,"new_interface_(.+)")
      if new_if_name then
        importInterface(new_if_name,content)
      elseif arg == "del_iface" then
        deleteInterface(content)
      elseif arg == "enabled" and content == "1" and getintf ~= "wg0" then
        checkForMultipleEnabledClients()
      end
    end
  end
end

local curintf = "wg0"
local wg0_on = ""
local wg_intfs = {}
local wg_intfs_path = "uci.wireguard."
local all_intfs = content_helper.convertResultToObject(wg_intfs_path.."@.",proxy.get(wg_intfs_path))
for _,v in ipairs(all_intfs) do
  local on = v.enabled == "0" and "" or "<sup class='green' style='font-variant:small-caps;'>&nbsp;on</sup>"
  if v.paramindex == getintf then
    curintf = v.paramindex
  end
  if v.paramindex == "wg0" then
    wg0_on = on
  else
    wg_intfs[#wg_intfs + 1] = {
      name = v.paramindex,
      on = on,
    }
  end
end

local function validateLogLevel(value,_,_)
  if value == "none" or value == "debug" then
    return true
  end
  return nil,T"'debug' or 'none' expected."
end

local log_level_attr = {
  values = {
    on = "debug",
    off = "none"
  }
}
local private_ip_attr = {
  span = {
    style = "letter-spacing:-1px",
  }
}

local mapParams = {
  enabled = "uci.wireguard.@"..curintf..".enabled",
  private_key = "uci.wireguard.@"..curintf..".private_key",
  addresses = "uci.wireguard.@"..curintf..".addresses",
  log_level = "uci.wireguard.@"..curintf..".log_level",
}

local mapValid = {
  enabled = vB,
  private_key = vNES,
  log_level = validateLogLevel,
}

if curintf == "wg0" then
  mapParams["ipv6"] = "uci.wireguard.@wg0.ipv6"
  mapParams["listen_port"] = "uci.wireguard.@wg0.listen_port"
  mapValid["ipv6"] = vB
  mapValid["listen_port"] = vSIP
end

local config,helpmsg
if deleting then
  config = mapParams
  content_helper.getExactContent(config)
else
  config,helpmsg = post_helper.handleQuery(mapParams,mapValid)
end
if not helpmsg then
  helpmsg = {}
end

if config.enabled ~= "0" then
  config.enabled = "1"
end
if config.log_level ~= "debug" then
  config.log_level = "none"
end

local server_addresses = {}
local ddns_domains = 0
if curintf == "wg0" then
  local function addServerAddress(v,d)
    local addr
    if find(v,":",1,true) then
      addr = format("[%s]",v)
    else
      addr = v
    end
    local index = #server_addresses+1
    server_addresses[index] = { format("%s:%s",addr,config.listen_port),T(format("%s %s",v,d)) }
    return index
  end

  if config.ipv6 == "" then
    config.ipv6 = "1"
  end
  if config.listen_port == "" then
    config.listen_port = "51820"
  end

  local ddns = proxy.getPN("uci.ddns.service.",true)
  if ddns then
    local family = { wan = "IPv4", wan6 = "IPv6", both = "(IPv4/IPv6)", }
    local disabled = {}
    local server_index = {}
    for k=1,#ddns do
      local path = ddns[k].path
      local v = proxy.get(path.."domain",path.."enabled",path.."interface")
      if v and v[1].value ~= "yourhost.example.com" and v[1].value ~= "" then
        local ifname = untaint(v[3].value)
        if ifname == "wan" or (config.ipv6 == "1" and ifname == "wan6") then
          local domain = untaint(v[1].value)
          if v[2].value ~= "0" then
            local index = server_index[domain]
            if index then
              local f = server_addresses[index][2]
              if not find(untaint(f),family[ifname],1,true) then
                server_addresses[index][2] = T(format("%s %s",domain,family.both))
              end
            else
              server_index[domain] = addServerAddress(domain,"("..family[ifname]..")")
            end
          else
            disabled[#disabled+1] = domain
          end
        end
      end
    end
    for k=1,#disabled do
      if not server_index[disabled[k]] then
        addServerAddress(disabled[k],"*DISABLED*")
      end
    end
  end
  ddns_domains = #server_addresses

  if config.ipv6 == "1" then
    local wan6_ip = proxy.get("rpc.network.interface.@wan6.ipv6uniqueglobaladdr")
    if wan6_ip and wan6_ip[1].value ~= "" then
      addServerAddress(gsub(untaint(wan6_ip[1].value),"/%d+",""),"(WAN)")
    end
    wan6_ip = proxy.get("rpc.network.interface.@wwan_6.ipv6uniqueglobaladdr")
    if wan6_ip and wan6_ip[1].value ~= "" then
      addServerAddress(gsub(untaint(wan6_ip[1].value),"/%d+",""),"(WAN)")
    end
    wan6_ip = proxy.get("rpc.network.interface.@lan.ipv6uniqueglobaladdr")
    if wan6_ip and wan6_ip[1].value ~= "" then
      addServerAddress(gsub(untaint(wan6_ip[1].value),"/%d+",""),"(LAN)")
    end
  end
  local wan_ip = proxy.get("rpc.network.interface.@wan.ipaddr")
  if wan_ip and wan_ip[1].value ~= "" then
    addServerAddress(untaint(wan_ip[1].value),"(WAN)")
  end
  wan_ip = proxy.get("rpc.network.interface.@wwan_4.ipaddr")
  if wan_ip and wan_ip[1].value ~= "" then
    addServerAddress(untaint(wan_ip[1].value),"(WWAN)")
  end

  if (#server_addresses - ddns_domains) < 1 then
    if config.enabled == "1" then
      message_helper.pushMessage(T"Disabled because no WAN IP found!","error")
      config.enabled = "0"
      proxy_helper.set("uci.wireguard.@"..curintf..".enabled",config.enabled)
    end
  else
    config.server_address = server_addresses[1][0]
  end
end

local new_peer
if ngx.var.request_method == "POST" then
  local args = ngx.req.get_post_args()
  if args.action == "SAVE" then
    if curintf == "wg0" then
      updateFirewall(args)
    end
    config.server_address = args.server_address
    if args.new_peer_name and args.new_peer_name ~= "" then
      if (#server_addresses - ddns_domains) < 1 then
        helpmsg["new_peer_name"] = "Cannot add peer: No WAN IP found"
      else
        local peer_name = untaint(args.new_peer_name)
        local valid,errmsg = vUN(peer_name)
        if valid then
          local ipv4_next,ipv4_netmask,ipv6_next,ipv6_netmask = wireguard_helper.getUnusedIP()
          if not ipv4_next then
            message_helper.pushMessage(T(ipv4_netmask),"error")
          else
            local _,errmsg = proxy.add("uci.wireguard.@"..curintf..".peer.",peer_name)
            if errmsg then
              for _,err in ipairs(errmsg) do
                message_helper.pushMessage(T(string.format("Failed to set %s to '%s': %s (%s)",err.path,peer_name,err.errmsg,err.errcode)),"error")
              end
              helpmsg["new_peer_name"] = "Failed to add peer"
            else
              local private_key = wireguard_helper.genkey()
              local include_ipv6 = (config.ipv6 ~= "0")
              new_peer = {
                name = peer_name,
                address = format("%s/%s",ipv4_next,ipv4_netmask),
                allowed_ips = format("%s/32",ipv4_next),
                ipv6_allowed_ip = "",
                private_key = private_key,
                public_key = wireguard_helper.pubkey(private_key),
                preshared_key = "",
                dns = wireguard_helper.getDNS(include_ipv6),
              }
              if include_ipv6 and ipv6_next then
                new_peer.address=format("%s,%s/%s",new_peer.address,ipv6_next,ipv6_netmask)
                new_peer.allowed_ips=format("%s,%s/128",new_peer.allowed_ips,ipv6_next)
                new_peer.ipv6_allowed_ip = ",::0/0"
              end
              if type(args["genpsk"]) == "table" then
                for _,checked in pairs(args["genpsk"]) do
                  if checked == "_TRUE_" then
                    local psk,psk_failed = wireguard_helper.genpsk()
                    if not psk then
                      message_helper.pushMessage(T(psk_failed),"error")
                    else
                      new_peer.preshared_key = psk
                    end
                    break
                  end
                end
              end
              if  proxy_helper.set(format("uci.wireguard.@"..curintf..".peer.@%s.public_key",peer_name),new_peer.public_key)
              and proxy_helper.set(format("uci.wireguard.@"..curintf..".peer.@%s.preshared_key",peer_name),new_peer.preshared_key)
              and proxy_helper.set(format("uci.wireguard.@"..curintf..".peer.@%s.allowed_ips",peer_name),new_peer.allowed_ips)
              and proxy_helper.set(format("uci.wireguard.@"..curintf..".peer.@%s.route_allowed_ips",peer_name),"1")
              then
                proxy.apply()
                local config_template ="[Interface]\nPrivateKey = %s\nAddress = %s\nDNS = %s\n\n[Peer]\nPublicKey = %s\nPresharedKey = %s\nAllowedIPs = 0.0.0.0/0%s\nEndpoint = %s\n"
                new_peer.config = gsub(format(config_template,new_peer.private_key,new_peer.address,new_peer.dns,wireguard_helper.pubkey(config.private_key),new_peer.preshared_key,new_peer.ipv6_allowed_ip,config.server_address),"PresharedKey = \n","")
              end
            end
          end
        else
          helpmsg["new_peer_name"] = errmsg
        end
      end
    end
  end
end

local peer_columns = {
  {
    header = T"Name",
    name = "paramindex",
    param = "paramindex",
    readonly = true,
  },
  {
    header = T"Public Key",
    name = "public_key",
    param = "public_key",
    readonly = true,
  },
  {
    header = T"PSK",
    name = "preshared_key",
    param = "preshared_key",
    readonly = true,
  },
  {
    header = T"Allowed IP",
    name = "allowed_ips",
    param = "allowed_ips",
    readonly = true,
  },
  {
    header = T"End Point",
    name = "endpoint",
    param = "endpoint",
    readonly = true,
  },
  {
    header = T"Latest Handshake",
    name = "latest_handshake",
    param = "latest_handshake",
    readonly = true,
  },
  {
    header = T"Rcvd",
    name = "transfer_rx",
    param = "transfer_rx",
    readonly = true,
  },
  {
    header = T"Sent",
    name = "transfer_tx",
    param = "transfer_tx",
    readonly = true,
  },
}

if curintf == "wg0" then
  peer_columns[#peer_columns+1] = {
    header = "LAN",
    name = "lan_access",
    param = "lan_access",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "right" }},
  }
  peer_columns[#peer_columns+1] = {
    header = "WAN",
    name = "wan_access",
    param = "wan_access",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "right" }},
  }
  peer_columns[#peer_columns+1] = {
    header = "SSH",
    name = "ssh_access",
    param = "ssh_access",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "right" }},
  }
  peer_columns[#peer_columns+1] = {
    header = "GUI",
    name = "gui_access",
    param = "gui_access",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "right" }},
  }
end

local rpc_path = "rpc.gui.wireguard.interface.@"..curintf..".peer."
local peers = content_helper.convertResultToObject(rpc_path,proxy.get(rpc_path)) or {}

local peer_valid = {
  paramindex = vNES,
  public_key = vNES,
  preshared_key = vNES,
  allowed_ips = vNES,
}

local peer_options = {
  tableid = "server",
  basepath = "uci.wireguard.@"..curintf..".peer.@.",
  canAdd = false,
  canDelete = (curintf == "wg0"),
  canEdit = (curintf == "wg0"),
}

local peer_filter = function(data)
  local pub = untaint(data.public_key)
  local psk = untaint(data.preshared_key)
  if data.endpoint_host == "" then
    data.endpoint = "(none)"
  else
    data.endpoint = format("%s:%s",data.endpoint_host,data.endpoint_port)
  end
  for _,peer in ipairs(peers) do
    if peer.public_key == pub then
      if peer.latest_handshake == "-62135596800" then
        data.latest_handshake = ""
      else
        data.latest_handshake = os.date("%Y-%m-%d %T",tonumber(untaint(peer.latest_handshake)))
      end
      data.transfer_rx = common.bytes2string(peer.transfer_rx)
      data.transfer_tx = common.bytes2string(peer.transfer_tx)
      if data.endpoint == "(none)" and peer.endpoint_host ~= "" then
        data.endpoint = format("%s:%s",peer.endpoint_host,peer.endpoint_port)
      end
      break
    end
  end
  data.public_key = format("%s...%s",sub(pub,1,4),sub(pub,40,43))
  if psk ~= "" then
    data.preshared_key = "Yes"
  end
  data.allowed_ips = gsub(untaint(data.allowed_ips),",","<br>")
  return true
end

local peer_data,peer_helpmsg = post_helper.handleTableQuery(peer_columns,peer_options,peer_filter,nil,peer_valid)

local active = ""
if curintf == "wg0" then
  active = "active"
end

local refresh_seconds
if not new_peer then
  refresh_seconds = 15
end

ngx.print(ui_helper.createHeader("WireGuard",false,true,refresh_seconds),'\
<div class="modal-body update">\
  <div class="span2">\
    <ul class="nav nav-list">\
      <li class="nav-header">Server</li>\
      <li class="',active,'"><a id="wg0" href="#" data-remote="modals/wireguard-modal.lp?intf=wg0">wg0',wg0_on,'</a></li>\
    </ul>\
    <ul class="nav nav-list">\
      <li class="nav-header">Client Interfaces</li>\
      ');
      if #wg_intfs == 0 then
        ngx.print('<li><i>None Configured</i></li>')
      else
        for _,v in ipairs(wg_intfs) do
          local url = "modals/wireguard-modal.lp?intf="..v.name
          active = ""
          if v.name == curintf then
            active = "active"
          end
          ngx.print(format('<li class="%s"><a id="%s" href="#" data-remote="%s">%s%s</a></li>',active,v.name,url,v.name,v.on))
        end
      end
      ngx.print('\
    </ul>\
    <br>\
    <ul id="conf-import" class="nav nav-list">\
      <li class="nav-header">Import <span class="icon-question-sign" title="You can import client configurations in .conf files and/or .zip files."></span></li>\
      <li><label class="btn btn-primary btn-large">\
        <input id="config-file" type="file" name="configfile" accept=".conf,.zip" multiple="multiple" style="display:none;">\
        <span style="font-size:small;">Browse for file</span></label>\
      </li>\
    </ul>\
  </div>\
  <style>\
  #server>tbody>tr>td:nth-child(n+4):nth-child(-n+5){letter-spacing:-1px;}\
  #server>tbody>tr>td:nth-child(n+7):nth-child(-n+8){text-align:right;}\
  </style>\
  <div class="span9">\
    <form class="form-horizontal" method="post" action="modals/wireguard-modal.lp?intf=',curintf,'">\
      ');
      local legend = T"VPN Server [wg0]"
      local enabled_label = T"Server Enabled"
      if curintf ~= "wg0" then
        legend = T(format("VPN Client %s",curintf))
        enabled_label = T"Enabled"
      end
      local html = {}
      html[#html+1] = ui_helper.createMessages(message_helper.popMessages())
      html[#html+1] = "<fieldset id='interface'><legend>"..legend.."</legend>"
      html[#html+1] = ui_helper.createSwitch(enabled_label,"enabled",config["enabled"],nil,helpmsg["enabled"])
      if config["enabled"] == "0" then
        html[#html+1] = '<script>$("li.active a").html("'..curintf..'");</script>'
      else
        html[#html+1] = '<script>$("li.active a").html("'..curintf..'<sup class=\\"green\\" style=\\"font-variant:small-caps;\\">&nbsp;on</sup>");</script>'
      end
      if curintf == "wg0" then
        html[#html+1] = ui_helper.createInputText(T"Listen Port","listen_port",config["listen_port"],{ input = { type="number",min="0",max="65535",style="width:100px",} },helpmsg["listen_port"])
        html[#html+1] = ui_helper.createSwitch(T"IPv6 Enabled","ipv6",config["ipv6"],nil,helpmsg["ipv6"])
        html[#html+1] = ui_helper.createInputSelect(T"Server Address<span class='icon-question-sign' title='Configure Domain names in WAN Services under Dynamic DNS'></span>","server_address",server_addresses,config.server_address)
      end
      html[#html+1] = ui_helper.createLabel(T"Private Key",config.private_key)
      html[#html+1] = ui_helper.createLabel(T"VPN Private IP",T(gsub(untaint(config.addresses),",","<br>")),private_ip_attr)
      html[#html+1] = ui_helper.createSwitch(T"Debugging","log_level",config["log_level"],log_level_attr,helpmsg["log_level"])
      html[#html+1] = "<legend><small>"..T"Peers".."</small></legend>"
      if curintf == "wg0"  and #server_addresses > 1 then
        html[#html+1] = ui_helper.createAlertBlock(T("Select the Server Address to be embedded in the configuration <i>before</i> creating a new Peer."),{ alert = { id = "new-peer-info",class = "alert-info" } })
      end
      html[#html+1] = ui_helper.createTable(peer_columns,peer_data,peer_options,nil,peer_helpmsg)
      if new_peer then
        html[#html+1] = ui_helper.createAlertBlock(T("Peer configuration is ONLY displayed on creation, because peer private keys are not stored and therefore the configuration cannot be regenerated. Click 'refresh data' above to clear configuration and add another peer."),{ alert = { id = "new-peer-alert",class = "alert-danger" } })
        html[#html+1] = '<script src="/js/qrcode.min.js"></script>'
        html[#html+1] = "<div style='float:right;'>"
        html[#html+1] = " <a id='wg-peer-config-download'>Download "..new_peer.name..".conf</a>"
        html[#html+1] = " <div id='wg-qrcode' style='padding:5px;background-color:white;'></div>"
        html[#html+1] = "</div>"
        html[#html+1] = "<pre id='wg-peer-config'>\n"
        html[#html+1] = new_peer.config
        html[#html+1] = "\n</pre>"
        html[#html+1] = ""
        html[#html+1] = '<script>\
        let config = document.getElementById("wg-peer-config").innerHTML;\
        let configAsBlob = new Blob([config],{type:"text/plain"});\
        let configAsURL = window.URL.createObjectURL(configAsBlob);\
        let downloadLink = document.getElementById("wg-peer-config-download");\
        downloadLink.download = "'..new_peer.name..'.conf";\
        downloadLink.href = configAsURL;\
        new QRCode("wg-qrcode",{ text:config,width:256,height:256,colorDark:"#000000",colorLight:"#ffffff",correctLevel:QRCode.CorrectLevel.M });\
        $("#new-peer-alert")[0].scrollIntoView();\
        </script>'
      elseif curintf == "wg0" and (#server_addresses - ddns_domains) > 0 then
        html[#html+1] = ui_helper.createInputText(T"Add new Peer","new_peer_name","",nil,helpmsg["new_peer_name"])
        html[#html+1] = ui_helper.createCheckboxSwitch(T"Generate PSK<span class='icon-question-sign' title='Generate a unique Pre-Shared Key for the new Peer to increase security.'></span>","genpsk","0")
      else
        html[#html+1] = ui_helper.createCheckboxSwitch(T("Remove "..curintf),"del_iface","0")
      end
      html[#html+1] = "</fieldset>"
      html[#html+1] = "<fieldset id='add-interface' class='hide'><legend>Add Interface</legend>"
      html[#html+1] = "<div id='interfaces-to-add'></div>"
      html[#html+1] = "</fieldset>"
      ngx.print(html,'\
    </form>\
  </div>\
</div>\
<script src="/js/modal-busy.js"></script>\
',ui_helper.createFooter(),'\
<script src="/js/jszip.min.js"></script>\
<script>\
const observer = new MutationObserver(function(mutations) {\
  $("#conf-import").hide();\
});\
const changes = document.querySelector("#modal-changes");\
observer.observe(changes,{\
  attributes: true\
});\
let target = $("#interfaces-to-add");\
$("#config-file").on("change",function(evt) {\
  target.html("");\
  function addNewInterface(filename,content) {\
    let name = filename.replace(/^\\/.+/g,"").replace(/\\.[^/.]+$/g,"").replace(/-/g,"_");\
    let group = $("<div>",{\
      "class" : "control-group"\
    });\
    group.append($("<label>",{\
      "class" : "control-label",\
      text : name\
    }));\
    let controls = $("<div>",{\
      "class" : "controls",\
    });\
    controls.append($("<pre>",{\
      "style" : "margin:6px 0px;",\
      text : content\
      }));\
    group.append(controls);\
    target.append(group);\
    target.append($("<input>",{\
      "type" : "hidden",\
      "name" : "new_interface_" + name,\
      "value" : content\
    }));\
  }\
  function handleFile(f) {\
    JSZip.loadAsync(f).then(function(zip) {\
      zip.forEach(function (relativePath,zipEntry) {\
        if (!zipEntry.dir) {\
          zipEntry.async("string").then(function(content) {\
            addNewInterface(zipEntry.name,content);\
          });\
        }\
      });\
    },function (e) {\
      let reader = new FileReader();\
      reader.readAsText(f);\
      reader.onload = function() {\
        addNewInterface(f.name,reader.result);\
      };\
      reader.onerror = function() {\
        target.append($("<div>",{\
            "class" : "alert alert-danger",\
            text : "Error reading " + f.name + ": " + e.message\
        }));\
      }\
    });\
  }\
  let files = evt.target.files;\
  for (let i = 0; i < files.length; i++) {\
      handleFile(files[i]);\
  }\
  $("#config-file").hide();\
  $("#interface").hide();\
  $("#add-interface").show();\
  $("#modal-no-change").hide();\
  $("#modal-changes").show();\
});\
$("#do-import").click(function(){\
  let form=$("#form-import");\
  let action=form.attr("action");\
  form.attr("action",action+"&filename="+encodeURIComponent($("#config_file")[0].files[0].name));\
  $("csrf_token").val($("meta[name=CSRFtoken]").attr("content"));\
  $.fileUpload(form,{\
    params: [ tch.elementCSRFtoken() ],\
    completeCallback: function(form,response) {\
    }\
  });\
});\
</script>\
');
