--pretranslated: do not change this file

-- NOTE: All occurences of the string "Device" in this file will be replaced with the variant (e.g. DJA0231)

-- Localization
gettext.textdomain('webui-core')

local bridged = require("bridgedmode_helper")
local content_helper = require("web.content_helper")
local ethernet_helper = require("ethernet_helper")
local lp = require("web.lp")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local proxy_helper = require("proxy_helper")
local ui_helper = require("web.ui_helper")

local ngx = ngx
local _,_,all_intfs,lan_intfs,_,_,_,_ = ethernet_helper.get_interfaces()

local ipairs,pairs,string = ipairs,pairs,string
local tonumber,tostring = tonumber,tostring
local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local isBridgedMode = bridged.isBridgedMode()

local vB = post_helper.validateBoolean
local vIP4N = post_helper.validateIPv4Netmask
local gOV = post_helper.getOptionalValidation
local gVNIR = post_helper.getValidateNumberInRange
local vNES = post_helper.validateNonEmptyString
local vSIIP = post_helper.validateStringIsIP
local gVSIDIP4 = post_helper.getValidateStringIsDeviceIPv4

local function addFirewallRule(name,src,proto,target,dest,dest_ip,dest_port,family)
  local okay = true
  local path
  if okay then
    local key,err = proxy_helper.add("uci.firewall.rule.")
    if err then
      okay = false
    else
      path = "uci.firewall.rule.@"..key.."."
      _,err = proxy.get(path)
      if err then
        path = "uci.firewall.rule."..key.."."
      end
    end
  end
  if okay then
    okay = okay and proxy_helper.set(path.."name",name)
    local index = proxy_helper.add(path.."proto.",nil,true)
    if not index then
      okay = okay and proxy_helper.set(path.."proto",proto) -- FW 17.2
    else
      okay = okay and proxy_helper.set(path.."proto.@"..index..".value",proto)
    end
    okay = okay and proxy_helper.set(path.."src",src)
    if dest then
      okay = okay and proxy_helper.set(path.."dest",dest)
    end
    if dest_ip then
      okay = okay and proxy_helper.set(path.."dest_ip",dest_ip)
    end
    if dest_port then
      okay = okay and proxy_helper.set(path.."dest_port",dest_port)
    end
    if family then
      okay = okay and proxy_helper.set(path.."family",family)
    end
    okay = okay and proxy_helper.set(path.."target",target)
  end
  return okay,path
end

local content = {
  cur_intf = "",
  ipaddr = "",
  netmask = "255.255.255.0",
  gateway = "",
  domain = "modem",
  ipv6 = "",
  ip6assign = "",
  ip6hint = "",
}
local helpmsg = {}

if ngx.var.request_method == "GET" then
  local used_ipaddr = {}
  local octet1,octet2,octet3,hint,assign
  local intfs = content_helper.convertResultToObject("uci.network.interface.@.",proxy.get("uci.network.interface."))
  for _,v in ipairs(intfs) do
    if v.type ~= "wan" and v.paramindex ~= "ppp" and v.paramindex ~= "ipoe" and v.name ~= "wg0" and v.proto ~= "wireguard" and v.ipaddr ~= "" then
      local ipaddr = untaint(v.ipaddr)
      used_ipaddr[ipaddr] = true
      if v.paramindex == "lan" then
        octet1,octet2,octet3 = match(ipaddr,"(%d+)%.(%d+)%.(%d+)%.(%d+)")
      end
      if v.ip6assign and v.ip6assign ~= "" then
        local ip6assign = tonumber(v.ip6assign)
        if ip6assign and (not assign or assign > ip6assign) then
          assign = ip6assign
        end
      end
      if v.ip6hint and v.ip6hint ~= "" then
        local ip6hint = tonumber(v.ip6hint)
        if ip6hint and (not hint or hint < ip6hint) then
          hint = ip6hint
        end
      end
    end
  end
  if octet3 then
    local next = tonumber(octet3)+1
    local ipaddr = format("%s.%s.%d.1",octet1,octet2,next)
    while used_ipaddr[ipaddr] do
      next = next + 1
      ipaddr = format("%s.%s.%d.1",octet1,octet2,next)
    end
    content.ipaddr = ipaddr
  end
  if assign then
    content.ip6assign = tostring(assign)
  else
    content.ip6assign = "64"
  end
  if hint then
    content.ip6hint = tostring(hint+1)
  end
elseif ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args(0)
  local valid,errmsg
  local apply = true

  content.cur_intf = untaint(post_data["cur_intf"])
  valid,errmsg = vNES(content.cur_intf)
  if not valid then
    helpmsg["cur_intf"] = errmsg
    apply = false
  elseif not match(content.cur_intf,"[_%w]+") then
    helpmsg["cur_intf"] = "Can only contain alphanumerics and underscores"
    apply = false
  else
    for _,v in ipairs(all_intfs) do
      if v.ifname == content.cur_intf then
        helpmsg["cur_intf"] = "Name already in use"
        apply = false
        break
      end
    end
  end

  content.ipaddr = untaint(post_data["ipaddr"])
  valid,errmsg = vSIIP(content.ipaddr)
  if not valid then
    helpmsg["ipaddr"] = errmsg
    apply = false
  else
    for _,v in ipairs(all_intfs) do

      if v.ipaddr == content.ipaddr then
        helpmsg["ipaddr"] = "IP Address already in use ("..v.paramindex..")"
        apply = false
        break
      elseif v.ipaddr and v.netmask and v.ipaddr ~= "" and v.netmask ~= "" then
        if gVSIDIP4(v.ipaddr,v.netmask)(content.ipaddr) then
          helpmsg["ipaddr"] = "IP Address conflicts with "..v.paramindex
          apply = false
          break
        end
      end
    end
  end

  content.netmask = untaint(post_data["netmask"])
  valid,errmsg = vIP4N(content.netmask)
  if not valid then
    helpmsg["netmask"] = errmsg
    apply = false
  end

  content.gateway = untaint(post_data["gateway"])
  if content.gateway and #content.gateway ~= 0 then
    valid,errmsg = vSIIP(content.gateway)
    if not valid then
      helpmsg["gateway"] = errmsg
      apply = false
    end
  end

  content.domain = untaint(post_data["domain"])
  valid,errmsg = vNES(content.domain)
  if not valid then
    helpmsg["domain"] = errmsg
    apply = false
  end

  content.ipv6 = untaint(post_data["ipv6"])
  valid,errmsg = vB(content.ipv6)
  if not valid then
    helpmsg["ipv6"] = errmsg
    apply = false
  end

  content.ip6assign = untaint(post_data["ip6assign"])
  valid,errmsg = gOV(gVNIR(48,64))(content.ip6assign)
  if not valid then
    helpmsg["ip6assign"] = errmsg
    apply = false
  end

  content.ip6hint = untaint(post_data["ip6hint"])
  valid,errmsg = gOV(ethernet_helper.validateHint)(content.ip6hint)
  if not valid then
    helpmsg["ipv6"] = errmsg
    apply = false
  end

  if apply then
    local okay = true
    local paths = {}

    paths[#paths+1] = "uci.network.interface.@"..content.cur_intf.."."
    local key,err = proxy_helper.add("uci.network.interface.",content.cur_intf)
    if err then
      okay = false
    end
    if okay then -- set interface config
      okay = okay and proxy_helper.set(paths[#paths].."proto","static")
      okay = okay and proxy_helper.set(paths[#paths].."force_link","1")
      okay = okay and proxy_helper.set(paths[#paths].."rpfilter","1")
      for k,v in pairs(content) do
        if k ~= "cur_intf" and k ~= "domain" then
          okay = okay and proxy_helper.set(paths[#paths]..k,v)
        end
      end
    end
    if okay then -- add dnsmasq section
      key,err = proxy_helper.add("uci.dhcp.dnsmasq.",content.cur_intf)
      if err then
        okay = false
      else
        paths[#paths+1] = "uci.dhcp.dnsmasq.@"..key.."."
      end
    end
    if okay then -- set dnsmasq config
      okay = okay and proxy_helper.set(paths[#paths].."disabled","0")
      okay = okay and proxy_helper.set(paths[#paths].."nonwildcard","1")
      okay = okay and proxy_helper.set(paths[#paths].."domainneeded","1")
      okay = okay and proxy_helper.set(paths[#paths].."boguspriv","1")
      okay = okay and proxy_helper.set(paths[#paths].."filterwin2k","0")
      okay = okay and proxy_helper.set(paths[#paths].."localise_queries","1")
      okay = okay and proxy_helper.set(paths[#paths].."rebind_protection","0")
      okay = okay and proxy_helper.set(paths[#paths].."rebind_localhost","1")
      okay = okay and proxy_helper.set(paths[#paths].."expandhosts","1")
      okay = okay and proxy_helper.set(paths[#paths].."nonegcache","0")
      okay = okay and proxy_helper.set(paths[#paths].."authoritative","1")
      okay = okay and proxy_helper.set(paths[#paths].."readethers","1")
      okay = okay and proxy_helper.set(paths[#paths].."leasefile","/tmp/dhcp.leases")
      okay = okay and proxy_helper.set(paths[#paths].."resolvfile","/tmp/resolv.conf.d/resolv.conf.auto")
      okay = okay and proxy_helper.set(paths[#paths].."strictorder","1")
      okay = okay and proxy_helper.set(paths[#paths].."dhcpscript","/lib/dnsmasq/dhcp-event.sh")
      okay = okay and proxy_helper.set(paths[#paths].."domain",content.domain)
      okay = okay and proxy_helper.set(paths[#paths].."add_local_fqdn","0")
      local dnsmasq_if,dnsmasq_if_err = proxy_helper.add(paths[#paths].."interface.")
      if dnsmasq_if_err then
        okay = false
      else
        okay = okay and proxy_helper.set(paths[#paths].."interface.@"..dnsmasq_if..".value",content.cur_intf)
      end
      local dnsmasq_notif,dnsmasq_notif_err = proxy_helper.add(paths[#paths].."notinterface.")
      if dnsmasq_notif_err then
        okay = false
      else
        okay = okay and proxy_helper.set(paths[#paths].."notinterface.@"..dnsmasq_notif..".value","loopback")
      end
    end
    if okay then -- add dhcp section
      key,err = proxy_helper.add("uci.dhcp.dhcp.",content.cur_intf.."_private")
      if err then
        okay = false
      else
        paths[#paths+1] = "uci.dhcp.dhcp.@"..key.."."
      end
    end
    if okay then -- set dhcp config
      okay = okay and proxy_helper.set(paths[#paths].."instance",content.cur_intf)
      okay = okay and proxy_helper.set(paths[#paths].."interface",content.cur_intf)
      okay = okay and proxy_helper.set(paths[#paths].."start","2")
      okay = okay and proxy_helper.set(paths[#paths].."limit","253")
      okay = okay and proxy_helper.set(paths[#paths].."leasetime","1d")
      okay = okay and proxy_helper.set(paths[#paths].."force","1")
      okay = okay and proxy_helper.set(paths[#paths].."dhcpv4","disabled")
      okay = okay and proxy_helper.set(paths[#paths].."dhcpv6","disabled")
      okay = okay and proxy_helper.set(paths[#paths].."ra","disabled")
      okay = okay and proxy_helper.set(paths[#paths].."ra_management","0")
      okay = okay and proxy_helper.set(paths[#paths].."ra_mininterval","200")
      okay = okay and proxy_helper.set(paths[#paths].."ra_maxinterval","600")
      okay = okay and proxy_helper.set(paths[#paths].."ra_lifetime","1800")
      okay = okay and proxy_helper.set(paths[#paths].."ra_hoplimit","64")
      okay = okay and proxy_helper.set(paths[#paths].."dhcpv4","disabled")
    end
    if okay then -- add firewall zone
      key,err = proxy_helper.add("uci.firewall.zone.")
      if err then
        okay = false
      else
        paths[#paths+1] = "uci.firewall.zone.@"..key.."."
        _,err = proxy.get(paths[#paths])
        if err then
          paths[#paths] = "uci.firewall.zone."..key.."."
        end
      end
    end
    if okay then -- set firewall zone config
      okay = okay and proxy_helper.set(paths[#paths].."name",content.cur_intf)
      okay = okay and proxy_helper.set(paths[#paths].."input","DROP")
      okay = okay and proxy_helper.set(paths[#paths].."output","ACCEPT")
      okay = okay and proxy_helper.set(paths[#paths].."forward","REJECT")
      okay = okay and proxy_helper.set(paths[#paths].."mtu_fix","1")
      okay = okay and proxy_helper.set(paths[#paths].."wan","0")
      local fw_zone_network = proxy_helper.add(paths[#paths].."network.")
      if not fw_zone_network then
        okay = false
      else
        okay = okay and proxy_helper.set(paths[#paths].."network.@"..fw_zone_network..".value",content.cur_intf)
      end
    end
    if okay then -- add firewall forwarding
      key,err = proxy_helper.add("uci.firewall.forwarding.")
      if err then
        okay = false
      else
        paths[#paths+1] = "uci.firewall.forwarding.@"..key.."."
        _,err = proxy.get(paths[#paths])
        if err then
          paths[#paths] = "uci.firewall.forwarding."..key.."."
        end
      end
    end
    if okay then -- set firewall forwarding config
      okay = okay and proxy_helper.set(paths[#paths].."src",content.cur_intf)
      okay = okay and proxy_helper.set(paths[#paths].."dest","wan")
    end
    if okay then -- add firewall rules
      okay,paths[#paths+1] = addFirewallRule("drop-lan-2-"..content.cur_intf,"lan","all","DROP",content.cur_intf)
    end
    if okay then -- add firewall rules
      okay,paths[#paths+1] = addFirewallRule("drop-"..content.cur_intf.."-2-lan",content.cur_intf,"all","DROP","lan")
    end
    if okay then -- add firewall rules
      okay,paths[#paths+1] = addFirewallRule("drop-lan-2-"..content.cur_intf.."_GW","lan","all","DROP",nil,content.ipaddr)
    end
    if okay then -- add firewall rules
      okay,paths[#paths+1] = addFirewallRule("Allow-"..content.cur_intf.."-ICMPv4",content.cur_intf,"icmp","ACCEPT",nil,content.ipaddr,nil,"ipv4")
    end
    if okay then -- add firewall rules
      okay,paths[#paths+1] = addFirewallRule("Allow-"..content.cur_intf.."-DHCP",content.cur_intf,"udp","ACCEPT",nil,nil,"67","ipv4")
    end
    if okay then -- add firewall rules
      okay,paths[#paths+1] = addFirewallRule("Allow-"..content.cur_intf.."-DNS",content.cur_intf,"udp","ACCEPT",nil,nil,"53","ipv4")
    end
    if okay then -- add firewall rules
      okay,paths[#paths+1] = addFirewallRule("Allow-"..content.cur_intf.."-ICMPv6",content.cur_intf,"icmp","ACCEPT",nil,nil,nil,"ipv6")
      okay = okay and proxy_helper.set(paths[#paths].."limit","1000/sec")
      for _,icmp_type in ipairs({"echo-request", "echo-reply", "destination-unreachable", "packet-too-big", "time-exceeded", "bad-header", "unknown-header-type", "router-solicitation", "neighbour-solicitation", "router-advertisement", "neighbour-advertisement"}) do
        local index = proxy_helper.add(paths[#paths].."icmp_type.")
        if not index then
          okay = false
          break
        else
          okay = okay and proxy_helper.set(paths[#paths].."icmp_type.@"..index..".value",icmp_type)
        end
      end
    end
    if okay then -- apply changes and redirect
      proxy.apply()
      message_helper.pushMessage(T(format("Added new LAN Interface '%s' and associated DHCP/firewall configuration",content.cur_intf)),"success")
      ngx.print("<div class='loading-wrapper'><img src='/img/spinner.gif'/></div><script>showLoadingWrapper();tch.loadModal('/modals/ethernet-modal.lp?intf=",content.cur_intf,"');</script>")
      ngx.exit(ngx.HTTP_OK)
    else -- delete all added paths
      for _,path in ipairs(paths) do
        ngx.log(ngx.ALERT,format("Deleting %s",path))
        local result,error = proxy.del(path)
        if not result then
          ngx.log(ngx.ERR,format("Failed to delete %s: %s",path,error))
        end
      end
    end
  end
end

ngx.print(ui_helper.createHeader(T"Local Network",true,true),'\
<div class="modal-body update">\
  '); lp.setpath("/www/snippets/"); lp.include("tabs-lan.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="',ngx.var.request_uri,'">\
    <div class="span2">\
      <ul class="nav nav-list">\
        <li class="nav-header">',T"LAN Interfaces",'</li>');
        local html = {}
        for _,v in ipairs(lan_intfs) do
          if v.index ~= "loopback" then
            local url = "modals/ethernet-modal.lp?intf="..v.index
            html[#html+1] = format('<li><a id="%s" href="#" data-remote="%s">%s</a></li>',v.index,url,v.name)
          end
        end
        ngx.print(html,'\
      </ul>\
    </div>\
    <div class="span9">\
      ',ui_helper.createMessages(message_helper.popMessages()),'\
      <div class="span4">\
        <legend>',T"Add LAN Interface",'</legend>\
        <fieldset>');
          --region Global
          --region attributes
          local advanced = {
            group = { class = "advanced" },
            input = { class = "span2" },
          }
          local switch_class_enable = {
            input = { class = "monitor-changes", },
          }
          local static_attr = {
            group = { class = "advanced monitor-proto monitor-static", },
            input = { class = "span2" },
          }
          --endregion attributes
          ngx.print(
            ui_helper.createInputText(T"Interface Name","cur_intf",content["cur_intf"],advanced,helpmsg["cur_intf"]),
            ui_helper.createInputText(T"Device IPv4 Address","ipaddr",content["ipaddr"],static_attr,helpmsg["ipaddr"]),
            ui_helper.createInputText(T"Local Network Subnet","netmask",content["netmask"],static_attr,helpmsg["netmask"])
          )
          if isBridgedMode then
            ngx.print(ui_helper.createInputText(T"Local Gateway","gateway",content["gateway"],static_attr,helpmsg["gateway"]))
          else
            ngx.print(ui_helper.createInputText(T"Local Domain Name","domain",content["domain"],advanced,helpmsg["domain"]))
          end

          ngx.print(ui_helper.createSwitch(T"IPv6 Enabled on Boot","ipv6",content["ipv6"],switch_class_enable))

          local min_prefix_len = 48
          local ip6prefix = proxy.get("rpc.network.interface.@wan6.ip6prefix")
          if ip6prefix and ip6prefix[1].value ~= "" then
            min_prefix_len = tonumber(match(untaint(ip6prefix[1].value),"/(%d+)$")) or 48
          end
          if min_prefix_len < 64 then min_prefix_len = min_prefix_len + 1 end
          local req_prefix_map = {}
          for prefix_len = 64,min_prefix_len,-1 do
            req_prefix_map[#req_prefix_map+1] = { tostring(prefix_len),T("/"..prefix_len) }
          end
          --region attributes
          local number_attr = {
            group = { class = "monitor-ipv6 monitor-1 monitor-hidden-ipv6", },
            select = { style = "width:100px", }
          }
          local hex_attr = {
            group = { class = "monitor-ipv6 monitor-1 monitor-hidden-ipv6", },
            input = { style = "width:80px", }
          }
          --endregion attributes
          ngx.print(
            ui_helper.createInputSelect(T"IPv6 Prefix Size<span class='icon-question-sign' title='Delegate a prefix of the given length to this interface'></span>","ip6assign",req_prefix_map,content["ip6assign"],number_attr,helpmsg["ip6assign"]),
            ui_helper.createInputText(T"IPv6 Subprefix Hint<span class='icon-question-sign' title='Subprefix ID to be used if available'></span>","ip6hint",content["ip6hint"],hex_attr,helpmsg["ip6hint"])
          )
            ngx.print('\
        </fieldset>\
      </div>\
      <div class="span4">\
      </div>\
    </div>\
  </form>\
  <script src="/js/modal-busy.js"></script>\
  <script src="/js/ethernet-modal.js"></script>\
  <script src="/js/srp-min.js"></script>\
  <script src="/js/bridgedmode.js"></script>\
</div>\
',ui_helper.createFooter())
