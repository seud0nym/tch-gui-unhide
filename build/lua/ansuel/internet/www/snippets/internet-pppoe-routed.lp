--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ajax_helper = require("web.ajax_helper")
local ui_helper = require("web.ui_helper")
local string = string
local format,match,find,upper = string.format,string.match,string.find,string.upper
local iconUp = "<i class=\"icon-upload icon-large gray\"></i> "
local iconDown = " <i class=\"icon-download icon-large gray\"></i> "
local untaint_mt = require("web.taint").untaint_mt

local ppp_state_map = {
  disabled = T"Disabled",
  disconnecting = T"Disconnecting",
  connected = T"Connected",
  connecting = T"Connecting",
  disconnected = T"Disconnected",
  error = T"Error",
  AUTH_TOPEER_FAILED = T"Authentication failed",
  NEGOTIATION_FAILED = T"Negotiation failed",
}

setmetatable(ppp_state_map,untaint_mt)

local ppp_light_map = {
  disabled = "off",
  disconnected = "red",
  disconnecting = "orange",
  connecting = "orange",
  connected = "green",
  error = "red",
  AUTH_TOPEER_FAILED = "red",
  NEGOTIATION_FAILED = "red",
}

setmetatable(ppp_light_map,untaint_mt)

local ajax_attr = {
  span = {
    ["data-ajaxrefresh"] = "5",
  }
}
local ajax_attr_light = {
  light = {
    class = "",
    id = "DHCP_Status_LED"
  },
  controls = {
    id = "wan_status",
    ["data-ajaxrefresh"] = "5",
  }
}

local function get_ppp_state(wan_auto,wan_ppp,wan_error,_)
  -- Figure out interface state
	if wan_auto ~= "0" then
    -- WAN enabled
    local ppp_state = wan_ppp
    if ppp_state == "" then
      -- Try again
      ppp_state = proxy.get("rpc.network.interface.@wan.ppp.state")[1].value
    end
	  if ppp_state == "" or ppp_state == "authenticating" or ppp_state == "networking" or (ppp_state == "connecting" and not(wan_error == "AUTH_TOPEER_FAILED")) then
      return "connecting"
		elseif ppp_state == "connected" or ppp_state == "disconnecting" or ppp_state == "disconnected" then
			return ppp_state
	  end
	  if not (wan_error == "" or wan_error == "USER_REQUEST") then
      if ppp_state_map[wan_error] then
        return wan_error
      else
        return "error"
      end
	  end
	else
	  -- WAN disabled
	  return "disabled"
	end
end

local function split(str,ang)
	local tab = {}
	for vari in string.gmatch(format("%s",str),ang) do
		table.insert(tab,vari)
	end
	return tab
end

local function dns_filter(tab)
	local ipv4_dns = ""
	local ipv6_dns = ""
	for _,v in ipairs(tab) do
		if match(v,"^(%d+)%.(%d+)%.(%d+)%.(%d+)$") then
			ipv4_dns=ipv4_dns..format("%s,",v)
		else
			ipv6_dns=ipv6_dns..format("%s,",v)
		end
	end
	if #ipv4_dns > 1 then
		ipv4_dns = string.sub(ipv4_dns,1,#ipv4_dns-1)
	end
	if #ipv6_dns > 1 then
		ipv6_dns = string.sub(ipv6_dns,1,#ipv6_dns-1)
	end
	return ipv4_dns,ipv6_dns
end

local firewall_zone = proxy.get("uci.firewall.zone.")
local firewall_zone_path = content_helper.convertResultToObject("uci.firewall.zone.",firewall_zone)
local zonenum = ""
for _,v in ipairs(firewall_zone_path) do
  if (v["name"] == "wan") then
    zonenum = v["paramindex"]
    break
  end
end
local firewall_forwarding = proxy.get("uci.firewall.forwarding.")
local firewall_forwarding_path = content_helper.convertResultToObject("uci.firewall.forwarding.",firewall_forwarding)
local forwdnum = ""
for _,v in ipairs(firewall_forwarding_path) do
  if (v["src"] == "wan" and v["dest"] == "lan") then
    forwdnum = v["paramindex"]
    break
  end
end
local function validateNat(value,_,_)
  local valid,msg = post_helper.validateBoolean(value)
  if valid and zonenum ~= "" and value ~= proxy.get("uci.firewall.zone."..zonenum..".masq")[1].value then
    proxy.set("uci.firewall.zone."..zonenum..".conntrack","1")
    local fwd_enabled
    if value == "1" then
      fwd_enabled = "0"
    else
      fwd_enabled = "1"
    end
    if forwdnum == "" then
      forwdnum = proxy.add("uci.firewall.forwarding.")
      -- 106-BEGIN the build will delete this block in FW 17.2.x,because it is only required for FW 18.1.c (possible transformer change - return value is now _key,@forwarding[index] as in FW 17)
      local count = 0
      firewall_forwarding_path = content_helper.convertResultToObject("uci.firewall.forwarding.",firewall_forwarding)
      for _ in ipairs(firewall_forwarding_path) do
        count = count + 1
      end
      forwdnum = "@forwarding["..count.."]"
      -- 106-END
      proxy.set("uci.firewall.forwarding."..forwdnum..".src","wan")
      proxy.set("uci.firewall.forwarding."..forwdnum..".dest","lan")
      proxy.set("uci.firewall.forwarding."..forwdnum..".name","for_w2l")
      proxy.set("uci.firewall.forwarding."..forwdnum..".enabled",fwd_enabled)
    else
      proxy.set("uci.firewall.forwarding."..forwdnum..".enabled",fwd_enabled)
    end
    proxy.apply()
  end
  return valid,msg
end

local mapParams = {
  uci_wan_auto = "uci.network.interface.@wan.auto",
  uci_wan_username = "uci.network.interface.@wan.username",
  uci_wan_password = "uci.network.interface.@wan.password",
  uci_ppp_username = "uci.network.interface.@ppp.username",
  uci_ppp_password = "uci.network.interface.@ppp.password",
  uci_wan_masq = "uci.firewall.zone."..zonenum..".masq",
  uci_wan_ipv6 = "uci.network.interface.@wan.ipv6",
  uci_wan6_auto = "uci.network.interface.@wan6.auto",
  uci_wan6_forceprefix = "uci.network.interface.@wan6.forceprefix",
  uci_wan_mac = "uci.network.interface.@wan.macaddr",
  uci_wan6_reqaddress = "uci.network.interface.@wan6.reqaddress",
  uci_wan6_reqprefix = "uci.network.interface.@wan6.reqprefix",
  uci_wan6_ifname = "uci.network.interface.@wan6.ifname",
}

local mapPPPstats = {
  uptime = "rpc.network.interface.@wan.uptime",
  discardR = "sys.class.net.@pppoe-wan.statistics.rx_dropped",
  discardS = "sys.class.net.@pppoe-wan.statistics.tx_dropped",
  broadcastR = "sys.class.net.@pppoe-wan.statistics.rx_broadcast",
  broadcastS = "sys.class.net.@pppoe-wan.statistics.tx_broadcast",
  --multicastR = "sys.class.net.@pppoe-wan.statistics.multicast",
  multicastS = "sys.class.net.@pppoe-wan.statistics.txpckt",
  packetsR = "sys.class.net.@pppoe-wan.statistics.rx_packets",
  packetsS = "sys.class.net.@pppoe-wan.statistics.tx_packets",
  errorsR = "sys.class.net.@pppoe-wan.statistics.rx_errors",
  errorsS = "sys.class.net.@pppoe-wan.statistics.tx_errors",
  bytesR = "sys.class.net.@pppoe-wan.statistics.rx_bytes",
  bytesS = "sys.class.net.@pppoe-wan.statistics.tx_bytes",
  unicastR = "sys.class.net.@pppoe-wan.statistics.rx_unicast",
  unicastS = "sys.class.net.@pppoe-wan.statistics.tx_unicast",
  unknownR = "sys.class.net.@pppoe-wan.statistics.rxerr",
}

content_helper.getExactContent(mapPPPstats)

-- Shortcuts to validation helpers to make lines shorter
local gCV = post_helper.getConditionalValidation
local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString
local gVP = post_helper.getValidationPassword
local gOV = post_helper.getOptionalValidation

local function validateUsername(value,postdata)
  local err,msg = vNES(value)
  if not err then
    return err,msg
  end
  local content_uci = {
    wan_domain = "uci.network.interface.@wan.domain",
  }
  content_helper.getExactContent(content_uci)
  if content_uci["wan_domain"] ~= "" then
    if find(value,"@") then
      return nil,T"Invalid username, domain locked"
    end
    postdata["uci_wan_username"] = format('%s@%s',value,content_uci["wan_domain"])
  end
  return true
end

local function isWanEnabled(_,object,_)
  return object["uci_wan_auto"] ~= "0"
end

local function IPv6andUsername(value,_,_)
	local valid,msg = vB(value)
	local ipv6_state = proxy.get("uci.network.interface.@wan6.auto")[1].value
	if valid and ipv6_state ~= value then
    local ipv6_realm = proxy.get("uci.env.var.ppp_realm_ipv6")
    local ipv4_realm = proxy.get("uci.env.var.ppp_realm_ipv4")
		if value == "1" then
			proxy.set("uci.network.interface.@wan6.auto","1")
			if ipv6_realm then
				proxy.set("uci.network.interface.@wan.username",ipv6_realm[1].value)
			end
			proxy.set("uci.network.device.@waneth4.ipv6","1")
			proxy.set("uci.network.device.@wanptm0.ipv6","1")
		else
			proxy.set("uci.network.interface.@wan.ipv6","0")
			proxy.set("uci.network.interface.@wan6.auto","0")
			if ipv4_realm then
				proxy.set("uci.network.interface.@wan.username",ipv4_realm[1].value)
			end
			proxy.set("uci.network.device.@waneth4.ipv6","0")
			proxy.set("uci.network.device.@wanptm0.ipv6","0")
		end
  	proxy.apply()
	end
	return valid,msg
end

local ipv6_ppp_map = {
  { "0",T"IPv6 disabled" },
  { "1",T"IPv6 enabled" },
  { "auto",T"Both IPv6 and DHCPv6 client enabled (auto)" }
}

local reqaddress_map = {
  { "try",T"Try" },
  { "force",T"Force" },
  { "none",T"None" },
}

local reqprefix_map = {
  { "no",T"No" },
  { "auto",T"Automatic" },
}
for prefix_len = 64,0,-1 do
  reqprefix_map[#reqprefix_map+1] = { tostring(prefix_len),T("/"..prefix_len) }
end

local ipv4ifname = proxy.get("uci.network.interface.@wan.ifname")[1].value
local ipv6ifname = proxy.get("uci.network.interface.@wan6.ifname")[1].value
local ifname_map = {
  { "@wan",T"@wan" },
}
if ipv6ifname == "" or ipv6ifname == "@wan" or ipv6ifname == ipv4ifname then
  ifname_map[#ifname_map+1] = { ipv4ifname,T(ipv4ifname) }
end

-- Validation allows empty values if wan access disabled but will stil check for validity if the parameter is set
-- if wan is enabled then it checks for validaty anyway
local mapValid = {
  uci_wan_auto = vB,
  uci_wan_username = gCV(isWanEnabled,validateUsername,gOV(validateUsername)),
  uci_wan_password = gCV(isWanEnabled,gVP(vNES),gVP()),
  uci_ppp_username = function(_,object)
    object.uci_ppp_username = object.uci_wan_username
    return true
  end,
  uci_ppp_password = function(_,object)
    object.uci_ppp_password = object.uci_wan_password
    return true
  end,
  uci_wan_masq = validateNat,
  uci_wan6_auto = IPv6andUsername,
	uci_wan6_forceprefix = vB,
  uci_wan_mac = gOV(post_helper.validateStringIsMAC),
  uci_wan_ipv6 = gVIES(ipv6_ppp_map),
  uci_wan6_reqaddress = gVIES(reqaddress_map),
  uci_wan6_reqprefix = gVIES(reqprefix_map),
  uci_wan6_ifname = gVIES(ifname_map),
}

local content,helpmsg = post_helper.handleQuery(mapParams,mapValid)
-- End POST process

if content["uci_wan6_ifname"] == "" then
  content["uci_wan6_ifname"] = ipv4ifname
end

local content_uci = {
  wan_proto = "uci.network.interface.@wan.proto",
  wan_domain = "uci.network.interface.@wan.domain",
}
content_helper.getExactContent(content_uci)

local content_rpc = {
  wan_ppp_state = "rpc.network.interface.@wan.ppp.state",
  wan_ppp_error = "rpc.network.interface.@wan.ppp.error",
  wan_ppp_ll_intf = "rpc.network.interface.@wan.ppp.ll_intf",
  ipaddr = "rpc.network.interface.@wan.ipaddr",
  nexthop = "rpc.network.interface.@wan.nexthop",
  dns = "rpc.network.interface.@wan.dnsservers",
  concentrator_name = "rpc.network.interface.@wan.ppp.access_concentrator_name",
  concentrator_mac = "rpc.network.interface.@wan.ppp.access_concentrator_mac",
  ipv6_gua = "rpc.network.interface.@wan6.ipv6uniquelocaladdr",
  dns_loopback = "rpc.network.interface.@loopback.dnsservers",-- Added to include static dns servers configured on loopback interface (for TI custo)
  ip6addr = "rpc.network.interface.@wan6.ip6addr",
  ip6prefix = "rpc.network.interface.@wan6.ip6prefix",
  dnsv6 = "rpc.network.interface.@wan6.dnsservers",
}


ajax_helper.handleAjaxQuery(content_rpc,function(c)
    -- this will add the HTML for the light
  local ds = get_ppp_state(content["uci_wan_auto"],proxy.get("rpc.network.interface.@wan.ppp.state")[1].value,proxy.get("rpc.network.interface.@wan.ppp.error")[1].value,c["ipaddr"])
	local dnstab = split(proxy.get("rpc.network.interface.@loopback.dnsservers")[1].value,"[^%,]+")
	local dnsv4,dnsv6 = dns_filter(dnstab)
  ajax_attr_light.light.class = ppp_light_map[ds]
  c.wan_status = ui_helper.createSimpleLight(nil,ppp_state_map[ds],ajax_attr_light)
	if proxy.get("rpc.network.interface.@wan.ppp.state")[1].value == "connected" then
		c.dns = content_helper.getMergedList(proxy.get("rpc.network.interface.@wan.dnsservers")[1].value,dnsv4)
	else
		c.dns = dnsv4
  end
  local ipv6_enabled = proxy.get("uci.network.interface.@wan.ipv6")
  if ipv6_enabled and ipv6_enabled[1].value == "1" then
    c.dnsv6 = dnsv6
  end
  c.discard = format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.discardS,iconDown,mapPPPstats.discardR)
	c.broadcast = format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.broadcastS,iconDown,mapPPPstats.broadcastR)
	--c.multicast = format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.multicastS,iconDown,mapPPPstats.multicastR)
	c.unicast = format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.unicastS,iconDown,mapPPPstats.unicastR)
	c.packets = format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.packetsS,iconDown,mapPPPstats.packetsR)
	c.error = format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.errorsS,iconDown,mapPPPstats.errorsR)
	c.byte =  format("%s %s [B] %s %s [B]",iconUp,mapPPPstats.bytesS,iconDown,mapPPPstats.bytesR)
	c.unknown = format("%s %s [Pkt]",iconDown,mapPPPstats.unknownR)
end)
-- will stop in handleAjaxQuery if it is an actual AJAX query
content_helper.getExactContent(content_rpc)

if content_rpc.ipaddr == "" then
  content_rpc.ipaddr = proxy.get("rpc.network.interface.@wan.ipaddr")[1].value
end
if content_rpc.nexthop == "" then
  content_rpc.nexthop = proxy.get("rpc.network.interface.@wan.nexthop")[1].value
end
if content_rpc.dns == "" then
  content_rpc.dns = proxy.get("rpc.network.interface.@wan.dnsservers")[1].value
end

local dnstab = split(content_rpc.dns,"[^%,]+")
local _,dnsv6 = dns_filter(dnstab)

local ipv6_enabled = proxy.get("uci.network.interface.@wan.ipv6")
if ipv6_enabled and ipv6_enabled[1].value == "1" then
	if not content_rpc.dnsv6 then
		content_rpc.dnsv6 = dnsv6
	end
end

local wan_atm_uci_path = format("uci.xtm.atmdevice.@%s.",content_rpc["wan_ppp_ll_intf"])
local wan_ppp_over_atm = proxy.get(wan_atm_uci_path) ~= nil
local content_uci_xtm
if wan_ppp_over_atm then
  content_uci_xtm = {
    wan_atm_vpi = wan_atm_uci_path.."vpi",
    wan_atm_vci = wan_atm_uci_path.."vci",
    wan_atm_enc = wan_atm_uci_path.."enc",
    wan_atm_ulp = wan_atm_uci_path.."ulp",
    wan_vpi = "uci.network.interface.@wan.vpi",
    wan_vci = "uci.network.interface.@wan.vci",
  }
  content_helper.getExactContent(content_uci_xtm)

  local params = {}
  local setting = false
  if content_uci_xtm.wan_vpi ~= "" then
    params["uci.network.interface.@wan.vpi"] = ""
    setting = true
  end
  if content_uci_xtm.wan_vci ~= "" then
    params["uci.network.interface.@wan.vci"] = ""
    setting = true
  end
  if content_uci_xtm.wan_atm_ulp ~= "eth" then
    params[wan_atm_uci_path.."ulp"] = "eth"
    setting = true
  end
  if content_uci_xtm.wan_atm_enc ~= "llc" then
    params[wan_atm_uci_path.."enc"] = "llc"
    setting = true
  end

  if setting then
    proxy.set(params)
    proxy.apply()
  end
end

if content_uci["wan_domain"] ~= "" then
  content["uci_wan_username"] = match(content["uci_wan_username"],"[^@]*")
end

local ppp_status = get_ppp_state(content.uci_wan_auto,format("%s",content_rpc.wan_ppp_state),format("%s",content_rpc.wan_ppp_error),content_rpc.ipaddr) -- use format to untaint

local supervision_support = proxy.get("uci.supervision.global.enabled")
local supervision_modes = {
  { "BFD",T"Bidirectional Forward Detection" },
  { "DNS",T"DNS" },
  { "Disabled",T"Disabled" },
}
local supervision_modes_content

if supervision_support then
  local supervision_modes_param = {
    supervision_mode = "uci.supervision.global.mode",
    supervision_enabled = "uci.supervision.global.enabled",
  }
  local supervision_modes_valid = {
    supervision_mode = post_helper.getValidateInEnumSelect(supervision_modes),
    supervision_enabled = post_helper.validateBoolean,
  }
  supervision_modes_content = post_helper.handleQuery(supervision_modes_param,supervision_modes_valid)
end

if content.uci_wan_auto ~= "0" then
  content.uci_wan_auto = "1"
end

if ngx.req.get_method() == "POST" then
  local doit = false
  local ipv6auto = proxy.get("uci.network.interface.@wan6.auto")[1].value

  if content.uci_wan6_auto ~= "0" then
    local ipv4proto = proxy.get("uci.network.interface.@wan.proto")[1].value
    local ipv6proto = proxy.get("uci.network.interface.@wan6.proto")[1].value

    if ipv6auto ~= content.uci_wan_auto then
      proxy.set("uci.network.interface.@wan6.auto",content.uci_wan_auto)
      doit = true
    end
    if ipv6proto ~= ipv4proto then
      proxy.set("uci.network.interface.@wan6.proto",ipv4proto)
      doit = true
    end
  elseif ipv6auto ~= "0" then
    proxy.set("uci.network.interface.@wan6.auto","0")
    doit = true
  end

  if doit then
    proxy.apply()
  end
end

  ngx.print('\
<fieldset>\
');
ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  ngx.print('\
<legend>');  ngx.print( T"PPP connection" ); ngx.print('</legend>\
');
local switch_class_enable = {
  input = {
    class = "monitor-changes",
  }
}
local ipv6_input = {
  group = {
    class ="monitor-uci_wan6_auto monitor-1",
  },
}
local html = {}
-- Label that shows PPP status
ajax_attr_light.light.class = ppp_light_map[ppp_status]
html[#html + 1] = ui_helper.createLight(T"Status",nil,ppp_state_map[ppp_status],ajax_attr_light)
ajax_attr.span.id = "uptime"
ajax_attr.span.class = "span4 simple-desc"
if not ( mapPPPstats["uptime"] == "" ) then
  html[#html + 1] = ui_helper.createLabel(T"Uptime",post_helper.secondsToTime(mapPPPstats["uptime"]),ajax_attr)
end
-- Switch PPP on/off
html[#html+1] = ui_helper.createSwitch(T"IPv4 Enabled on Boot","uci_wan_auto",content.uci_wan_auto,nil,helpmsg["uci_wan_auto"])
html[#html+1] = ui_helper.createSwitch(T"NAT","uci_wan_masq",content.uci_wan_masq,nil,helpmsg["uci_wan_masq"])
html[#html+1] = ui_helper.createSwitch(T"IPv6 Enabled on Boot","uci_wan6_auto",content.uci_wan6_auto,switch_class_enable,helpmsg["uci_wan6_auto"])
html[#html+1] = ui_helper.createInputSelect(T"IPv6 on the PPP link<span class='icon-question-sign' title='Behaviour of IPv6 on the PPP link. Recommended setting is auto when IPv6 enabled.'></span>","uci_wan_ipv6",ipv6_ppp_map,content.uci_wan_ipv6,ipv6_input)
html[#html+1] = ui_helper.createInputSelect(T"IPv6 Interface<span class='icon-question-sign' title='Some RSPs may require the interface to be set to @wan'></span>","uci_wan6_ifname",ifname_map,content.uci_wan6_ifname,ipv6_input)
html[#html+1] = ui_helper.createInputSelect(T"IPv6 Address Request<span class='icon-question-sign' title='Behaviour for requesting addresses. The device default is \"Force\",but some RSPs require it to be \"Try\".'></span>","uci_wan6_reqaddress",reqaddress_map,content.uci_wan6_reqaddress,ipv6_input)
html[#html+1] = ui_helper.createInputSelect(T"IPv6 Prefix Request<span class='icon-question-sign' title='Behaviour for requesting prefixes (numbers denote hinted prefix length). Use \"None\" if you only want a single IPv6 address for the AP itself without a subnet for routing.'></span>","uci_wan6_reqprefix",reqprefix_map,content.uci_wan6_reqprefix,ipv6_input)
html[#html+1] = ui_helper.createSwitch(T"IPv6 Force Prefix<span class='icon-question-sign' title='Require the presence of IPv6 Prefix in received DHCP message'></span>","uci_wan6_forceprefix",content.uci_wan6_forceprefix,ipv6_input)
-- Supervision
if supervision_support then
  html[#html+1] = ui_helper.createInputSelect(T"WAN Supervision","supervision_mode",supervision_modes,supervision_modes_content["supervision_mode"])
  html[#html+1] = ui_helper.createSwitch(T"Supervision Enabled","supervision_enabled",supervision_modes_content["supervision_enabled"])
end
-- PPP username
local class_username = {
  input = {
    maxlength= "128",
    class = "span6",
  }
}
html[#html + 1] = "<div class='control-group'><label class='control-label'>"..T"Username".."</label><div class='controls'>"
html[#html + 1] = ui_helper.createSimpleInputText("uci_wan_username",content["uci_wan_username"],class_username,helpmsg["uci_wan_username"])
if content_uci["wan_domain"] ~= "" then
  html[#html + 1] = " @ "..content_uci["wan_domain"]
end
html[#html + 1] = "</div></div>"
-- PPP password
html[#html + 1] = ui_helper.createInputText(T"Password","uci_wan_password",content["uci_wan_password"],class_username,helpmsg["uci_wan_password"])
html[#html + 1] = ui_helper.createInputText(T"MAC Spoof (Wan MAC)","uci_wan_mac",content["uci_wan_mac"],nil,helpmsg["uci_wan_mac"])
ajax_attr.span.id = "ipaddr"
html[#html+1] = ui_helper.createLabel(T"IPv4 address",content_rpc["ipaddr"],ajax_attr)
ajax_attr.span.id = "nexthop"
html[#html+1] = ui_helper.createLabel(T"Gateway",content_rpc["nexthop"],ajax_attr)
if content_rpc["concentrator_name"] ~= "" then
  ajax_attr.span.id = "concentrator_name"
  html[#html+1] = ui_helper.createLabel(T"Concentrator Name",content_rpc["concentrator_name"],ajax_attr)
end
ajax_attr.span.id = "concentrator_mac"
html[#html+1] = ui_helper.createLabel(T"Concentrator MAC",content_rpc["concentrator_mac"],ajax_attr)
ajax_attr.span.id = "dns"
html[#html+1] = ui_helper.createLabel(T"IPv4 DNS servers",content_rpc["dns"],ajax_attr)
if content_rpc["ip6addr"] and content_rpc["ip6addr"]:len() > 0 then
  ajax_attr.span.id = "ip6addr"
  html[#html+1] = ui_helper.createLabel(T"IPv6 Address",content_rpc["ip6addr"],ajax_attr)
end
if content_rpc["ipv6_gua"] and content_rpc["ipv6_gua"]:len() > 0 then
  ajax_attr.span.id = "ipv6_gua"
  html[#html+1] = ui_helper.createLabel(T"IPv6 GUA",content_rpc["ipv6_gua"],ajax_attr)
end
if content_rpc["ip6prefix"] and content_rpc["ip6prefix"]:len() > 0 then
  ajax_attr.span.id = "ip6prefix"
  html[#html+1] = ui_helper.createLabel(T"IPv6 Prefix",content_rpc["ip6prefix"],ajax_attr)
end
if content_rpc["dnsv6"] and content_rpc["dnsv6"]:len() > 0 then
  ajax_attr.span.id = "dnsv6"
  html[#html+1] = ui_helper.createLabel(T"IPv6 DNS Servers",content_rpc["dnsv6"],ajax_attr)
end

-- ATM VP/VC
if wan_ppp_over_atm then
  local advanced = {
    group = {
      class = "advanced hide",
    }
  }
  local vp_vc = format("%s / %s",content_uci_xtm["wan_atm_vpi"],content_uci_xtm["wan_atm_vci"])
  html[#html + 1] = ui_helper.createLabel(T"ATM VP / VC",vp_vc,advanced)
  html[#html + 1] = ui_helper.createLabel(T"Encapsulation",upper(content_uci_xtm["wan_atm_enc"]),advanced)
end

ngx.print(html)
ngx.print('</fieldset>');

if (ppp_status == "connected") then
  ngx.print('<fieldset>\
  <legend>');  ngx.print( T"PPP statistics" ); ngx.print('</legend>\
  ');
  local html = {}
  ajax_attr.span.id = "discard"
  ajax_attr.span.class = "span4 simple-desc"
  html[#html + 1] = ui_helper.createLabel(T"Discard Packets",format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.discardS,iconDown,mapPPPstats.discardR),ajax_attr)
  ajax_attr.span.id = "broadcast"
  html[#html + 1] = ui_helper.createLabel(T"Broadcast Packets",format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.broadcastS,iconDown,mapPPPstats.broadcastR),ajax_attr)
  --[[		ajax_attr.span.id = "multicast"
          html[#html + 1] = ui_helper.createLabel(T"Multicast Packets",format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.multicastS,iconDown,mapPPPstats.multicastR),ajax_attr)
    ]]
  ajax_attr.span.id = "unicast"
  html[#html + 1] = ui_helper.createLabel(T"Unicast Packets",format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.unicastS,iconDown,mapPPPstats.unicastR),ajax_attr)
  ajax_attr.span.id = "packets"
  html[#html + 1] = ui_helper.createLabel(T"Packets",format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.packetsS,iconDown,mapPPPstats.packetsR),ajax_attr)
  ajax_attr.span.id = "error"
  html[#html + 1] = ui_helper.createLabel(T"Errors",format("%s %s [Pkt] %s %s [Pkt]",iconUp,mapPPPstats.errorsS,iconDown,mapPPPstats.errorsR),ajax_attr)
  ajax_attr.span.id = "byte"
  html[#html + 1] = ui_helper.createLabel(T"Bytes",format("%s %s [B] %s %s [B]",iconUp,mapPPPstats.bytesS,iconDown,mapPPPstats.bytesR),ajax_attr)
  ajax_attr.span.id = "unknown"
  html[#html + 1] = ui_helper.createLabel(T"Unknown Proto Packets",format("%s %s [Pkt]",iconDown,mapPPPstats.unknownR),ajax_attr)

  ngx.print(html)
  ngx.print('\
  </fieldset>\
  ');
end
