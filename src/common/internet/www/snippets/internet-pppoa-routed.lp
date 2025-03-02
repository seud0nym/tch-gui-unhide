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
local session = ngx.ctx.session
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
	  if wan_ppp == "" or wan_ppp == "authenticating" or wan_ppp == "networking" or (wan_ppp == "connecting" and not(wan_error == "AUTH_TOPEER_FAILED")) then
		return "connecting"
		elseif wan_ppp == "connected" then
			return "connected"
		elseif wan_ppp == "disconnecting" then
			return "disconnecting"
		elseif wan_ppp == "disconned" then
			return "disconned"
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

local split = require("split").split

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
  uci_wan_masq = "uci.firewall.zone."..zonenum..".masq",
  uci_wan_ipv6 = "uci.network.interface.@wan.ipv6",
  uci_wan_peerdns = "uci.network.interface.@wan.peerdns",
}

-- Shortcuts to validation helpers to make lines shorter
local gCV = post_helper.getConditionalValidation
local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString
local vSII = post_helper.validateStringIsIP
local gVP = post_helper.getValidationPassword
local gOV = post_helper.getOptionalValidation

local dns_options = {
    tableid = "dns_servers",
    basepath = "uci.network.interface.@wan.dns.@.",
    createMsg = T"Add DNS server",
    minEntries = 0,
    maxEntries = 3,
}

local dns_columns = {
    {
        header = T"DNS IP",
        name = "dns_server",
        param = "value",
        type = "text",
        attr = { input = { class="no-save" } },
    }
}

local dns_valid = {
    dns_server = gOV(vSII)
}

local dns_data,dns_helpmsg = post_helper.handleTableQuery(dns_columns,dns_options,nil,nil,dns_valid)
session:store("dns_data",dns_data)

local function validateUsername (value,postdata)
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
  local ipv6_state = proxy.get("uci.network.interface.@wan.ipv6")[1].value
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

-- Validation allows empty values if wan access disabled but will stil check for validity if the parameter is set
-- if wan is enabled then it checks for validaty anyway
local mapValid = {
  uci_wan_auto = vB,
  uci_wan_username = gCV(isWanEnabled,validateUsername,gOV(validateUsername)),
  uci_wan_password = gCV(isWanEnabled,gVP(vNES),gVP()),
  uci_wan_masq = validateNat,
  uci_wan_ipv6 = IPv6andUsername,
  uci_wan_peerdns = vB,
}

local content,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if not helpmsg then helpmsg = {} end
-- End POST process

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
end)

content_helper.getExactContent(content_rpc)

local dnstab = split(content_rpc.dns,"[^%,]+")
local dnsv4,dnsv6 = dns_filter(dnstab)
content_rpc.dns = dnsv4
if dnsv6 and not content_rpc.dnsv6 then
	content_rpc.dnsv6 = dnsv6
end

-- need to untaint the string otherwise getExactContent will get lost
local wan_atm_uci_path = format("uci.xtm.atmdevice.@%s.",content_rpc["wan_ppp_ll_intf"])
local  content_uci_xtm = {
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
if content_uci_xtm.wan_atm_vpi ~= content_uci_xtm.wan_vpi then
  params["uci.network.interface.@wan.vpi"] = content_uci_xtm.wan_atm_vpi
  setting = true
end
if content_uci_xtm.wan_atm_vci ~= content_uci_xtm.wan_vci then
  params["uci.network.interface.@wan.vci"] = content_uci_xtm.wan_atm_vci
  setting = true
end
if content_uci_xtm.wan_atm_ulp ~= "ppp" then
  params[wan_atm_uci_path.."ulp"] = "ppp"
  setting = true
end

if setting then
  proxy.set(params)
  proxy.apply()
end

if content_uci["wan_domain"] ~= "" then
  content["uci_wan_username"] = match(content["uci_wan_username"],"[^@]*")
end

local ppp_status
if content.uci_wan_auto ~= "0" then
  -- WAN enabled
  content.uci_wan_auto = "1"
  ppp_status = format("%s",content_rpc.wan_ppp_state) -- untaint
  if ppp_status == "" or ppp_status == "authenticating" or ppp_status == "networking" then
    ppp_status = "connecting"
  end

  if not (content_rpc.wan_ppp_error == "" or content_rpc.wan_ppp_error == "USER_REQUEST") then
    if ppp_state_map[content_rpc.wan_ppp_error] then
      ppp_status = content_rpc.wan_ppp_error
    else
      ppp_status = "error"
    end
  end
else
  -- WAN disabled
  ppp_status = "disabled"
end

  ngx.print('\
<fieldset>\
');
ngx.print(ui_helper.createMessages(message_helper.popMessages()))
ngx.print('\
<legend>');  ngx.print( T"PPP connection" ); ngx.print('</legend>\
');

  local html = {}
  -- Label that shows PPP status
  ajax_attr_light.light.class = ppp_light_map[ppp_status]
  html[#html + 1] = ui_helper.createLight(T"Status",nil,ppp_state_map[ppp_status],ajax_attr_light)
  -- Switch PPP on/off
  html[#html+1] = ui_helper.createSwitch(T"IPv4 Enabled on Boot","uci_wan_auto",content.uci_wan_auto)
  html[#html + 1] = ui_helper.createSwitch(T"NAT","uci_wan_masq",content.uci_wan_masq)
  html[#html + 1] = ui_helper.createSwitch(T"IPv6","uci_wan_ipv6",content.uci_wan_ipv6)
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

ajax_attr.span.id = "ipaddr"
html[#html+1] = ui_helper.createLabel(T"IPv4 Address",content_rpc["ipaddr"],ajax_attr)
ajax_attr.span.id = "nexthop"
html[#html+1] = ui_helper.createLabel(T"IPv4 Gateway",content_rpc["nexthop"],ajax_attr)
ajax_attr.span.id = "dns"
html[#html+1] = ui_helper.createLabel(T"IPv4 DNS Servers",content_rpc["dns"],ajax_attr)
if content["uci_wan_peerdns"] == "" then
    content["uci_wan_peerdns"] = "1"
end
html[#html+1] = ui_helper.createSwitch(T"Auto Discover DNS","uci_wan_peerdns",content["uci_wan_peerdns"])
html[#html+1] = format('<label class="control-label">%s</label><div class="controls">',T"DNS servers")
html[#html+1] = ui_helper.createTable(dns_columns,dns_data,dns_options,nil,dns_helpmsg)
html[#html+1] = '</div>'

if content_rpc["ip6addr"] and content_rpc["ip6addr"]:len() > 0 then
	ajax_attr.span.id = "ip6addr"
	html[#html+1] = ui_helper.createLabel(T"IPv6 Addresses",content_rpc["ip6addr"],ajax_attr)
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
  local advanced = {
    group = {
      class = "advanced hide",
    }
  }
  local vp_vc = format("%s / %s",content_uci_xtm["wan_atm_vpi"],content_uci_xtm["wan_atm_vci"])
  html[#html + 1] = ui_helper.createLabel(T"ATM VP / VC",vp_vc,advanced)
  html[#html + 1] = ui_helper.createLabel(T"Encapsulation",upper(content_uci_xtm["wan_atm_enc"]),advanced)

  ngx.print(html)
  ngx.print('\
</fieldset>\
');
