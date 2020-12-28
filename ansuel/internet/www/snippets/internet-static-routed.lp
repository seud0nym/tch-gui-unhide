--pretranslated: do not change this file
 
---
--- ENABLE LOCALIZATION
---
gettext.textdomain('webui-core')

---
--- REQUIRED LIBRARIES
---
--- content_helper: extracts information from the data model / uci layer
--- ui_helper:      controls creation of web ui widgets (switches, text entry, radio buttons, etc)
--- post_helper:    controls behavior of POST messages sent from web ui to Transformer
--- proxy:          used for access to the data model / uci layer (get and set methods)
--- message_helper: produces human readable content in the web ui (e.g. errors, success prompts)
---
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local message_helper = require("web.uimessage_helper")
local ajax_helper = require("web.ajax_helper")

local ajax_attr = {
    span = {
      ["data-ajaxrefresh"] = "5",
    }
}

local firewall_zone = proxy.get("uci.firewall.zone.")
local firewall_zone_path = content_helper.convertResultToObject("uci.firewall.zone.",firewall_zone)
local zonenum = ""
local match, format = string.match, string.format

local function split(str, ang)
	local tab = {}
	for vari in string.gfind(format("%s",str), ang) do
		table.insert(tab, vari)
	end
	return tab
end 

local function dns_filter(tab)
	local ipv4_dns = ""
	local ipv6_dns = ""
	
	for _,v in ipairs(tab) do
		if match(v, "^(%d+)%.(%d+)%.(%d+)%.(%d+)$") then
			ipv4_dns=ipv4_dns..format("%s,", v)
		else
			ipv6_dns=ipv6_dns..format("%s,", v)
		end
	end
	
	if #ipv4_dns > 1 then
		ipv4_dns = string.sub(ipv4_dns,1,#ipv4_dns-1)
	end
	if #ipv6_dns > 1 then
		ipv6_dns = string.sub(ipv6_dns,1,#ipv6_dns-1)
	end
	return ipv4_dns, ipv6_dns
end

for _, v in ipairs(firewall_zone_path) do
  if (v["name"] == "wan") then
    zonenum = v["paramindex"]
    break
  end
end
local firewall_forwarding = proxy.get("uci.firewall.forwarding.")
local firewall_forwarding_path = content_helper.convertResultToObject("uci.firewall.forwarding.",firewall_forwarding)
local forwdnum = ""
for _, v in ipairs(firewall_forwarding_path) do
  if (v["src"] == "wan" and v["dest"] == "lan") then
    forwdnum = v["paramindex"]
    break
  end
end
local function validateNat(value, object, key)
  local valid, msg = post_helper.validateBoolean(value, object, key)
  if valid and zonenum ~= "" and value ~= proxy.get("uci.firewall.zone."..zonenum..".masq")[1].value then
    proxy.set("uci.firewall.zone."..zonenum..".conntrack", "1")
    local fwd_enabled
    if value == "1" then
      fwd_enabled = "0"
    else
      fwd_enabled = "1"
    end
    if forwdnum == "" then
      forwdnum = proxy.add("uci.firewall.forwarding.")
      -- 106-BEGIN the build will delete this block in FW 17.2.x, because it is only required for FW 18.1.c (possible transformer change - return value is now _key, @forwarding[index] as in FW 17)
      local count = 0
      firewall_forwarding_path = content_helper.convertResultToObject("uci.firewall.forwarding.",firewall_forwarding)
      for _ in ipairs(firewall_forwarding_path) do
        count = count + 1
      end
      forwdnum = "@forwarding[" .. count .. "]"
      -- 106-END
      proxy.set("uci.firewall.forwarding."..forwdnum..".src", "wan")
      proxy.set("uci.firewall.forwarding."..forwdnum..".dest", "lan")
      proxy.set("uci.firewall.forwarding."..forwdnum..".name", "for_w2l")
      proxy.set("uci.firewall.forwarding."..forwdnum..".enabled", fwd_enabled)
    else
      proxy.set("uci.firewall.forwarding."..forwdnum..".enabled", fwd_enabled)
    end
    proxy.apply()
  end
  return valid, msg
end
local function validateIPv6(value, object, key)
  local valid, msg = post_helper.validateBoolean(value, object, key)
  if valid and value == "1" then
    if object.uci_wan6_addr == "" or object.uci_wan6_gw == "" or object.uci_wan6_prefix == "" then
      return nil, "IPv6 Address, Gateway and LAN Prefix are required"
    end
  end
  return valid, msg
end

---
--- SETUP FOR PRINTING DATA
---

---
--- SHORTCUTS FOR DATA PATHS
---
local wanIpAddrPath_uci   = "uci.network.interface.@wan.ipaddr"
local wanNetmaskPath_uci  = "uci.network.interface.@wan.netmask"
local wanGatewayPath_uci  = "uci.network.interface.@wan.gateway"

---
--- CONFIGURABLE DATA FROM MAP FILE (network.map)
---
local mapParams = {
  wanIpAddrParam    = wanIpAddrPath_uci,
  wanNetmaskParam   = wanNetmaskPath_uci,
  wanGatewayParam   = wanGatewayPath_uci,
  uci_wan_auto = "uci.network.interface.@wan.auto",
  uci_wan_masq = "uci.firewall.zone."..zonenum..".masq",
  uci_wan_mac = "uci.network.interface.@wan.macaddr",
  uci_wan_ipv6 = "uci.network.interface.@wan.ipv6",
  uci_wan6_proto = "uci.network.interface.@wan6.proto",
  uci_wan6_addr = "uci.network.interface.@wan6.ip6addr",
  uci_wan6_gw = "uci.network.interface.@wan6.ip6gw",
  uci_wan6_prefix = "uci.network.interface.@wan6.ip6prefix",
}

---
--- FIELD VALUE INTEGRITY FUNCTIONS
---
local gAV = post_helper.getAndValidation
local nEIPV = post_helper.getAndValidation(post_helper.validateNonEmptyString,post_helper.advancedIPValidation)
local gOV = post_helper.getOptionalValidation
local vIAS6 = gOV(post_helper.validateIPAndSubnet(6))
local vSIIP6 = gOV(post_helper.validateStringIsIPv6)
local mapValid = {
  wanIpAddrParam    = gAV(nEIPV, post_helper.reservedIPValidation),
  wanNetmaskParam   = post_helper.validateIPv4Netmask,
  wanGatewayParam   = gAV(nEIPV, post_helper.reservedIPValidation),
  uci_wan_auto = post_helper.validateBoolean,
  uci_wan_masq = validateNat,
  uci_wan_mac = gOV(post_helper.validateStringIsMAC),
  uci_wan6_proto = post_helper.validateNonEmptyString,
  uci_wan_ipv6 = validateIPv6, 
  uci_wan6_addr = vIAS6,
  uci_wan6_gw = vSIIP6,
  uci_wan6_prefix = vIAS6,
}

--- GRAB CURRENT DATA AND HELP MESSAGES FROM POST HANDLER
--- FOR DNS CONFIGURATION, EITHER GET CURRENT VALUE OR LEAVE BLANK BASED ON FINDINGS
--- HELP MESSAGES APPEAR NEXT TO CONTROLS (EXAMPLE INVALID VALUE ERRORS FOR TEXT FIELDS)
local content, helpmsg = post_helper.handleQuery(mapParams, mapValid)

---
--- DNS CONFIGURATION TABLE ITEMS
---
local content_dns = {
  wandns = "rpc.network.interface.@wan.dnsservers";
  loopbackdns = "rpc.network.interface.@loopback.dnsservers";
}
						
ajax_helper.handleAjaxQuery(content_dns, function(c)
    -- this will add the HTML for the light
	local dnstab = split(c.loopbackdns, "[^%,]+")
	local dnsv4, dnsv6 = dns_filter(dnstab)
	c.loopbackdns = content_helper.getMergedList(proxy.get("rpc.network.interface.@wan.dnsservers")[1].value, dnsv4)
end)

content_helper.getExactContent(content_dns)


local dnstab = split(content_dns.loopbackdns, "[^%,]+")
local dnsv4, dnsv6 = dns_filter(dnstab)
content_dns.loopbackdns = content_helper.getMergedList(content_dns.wandns, dnsv4)

local ipv4pattern = "^(((([1]?\\d)?\\d|2[0-4]\\d|25[0-5])\\.){3}(([1]?\\d)?\\d|2[0-4]\\d|25[0-5]))$"
local dnsColumns = {
    {
       header = T"DNS IP Address",
       name   = "wanDnsParam",                          -- MAP THIS PARAMETER AGAINST dnsValid TABLE
       param  = "value",                                -- APPENDED TO basepath OF dnsOptions TABLE
       type   = "text",
       attr   = { input = { class="span2", maxlength="15", pattern = ipv4pattern } },
    },
}

local dnsOptions = {
    tableid    = "dnsserver",
    basepath   = "uci.network.interface.@wan.dns.@.",
    createMsg  = T"Add New DNS",
    minEntries = 1,
    maxEntries = 4,                                     -- ADJUST LIMIT AS SEEN FIT
    sorted = function(a, b)
        return tonumber(a.paramindex) < tonumber(b.paramindex)
    end
}

local dnsValid = {
    wanDnsParam =  post_helper.advancedIPValidation,
}

local dnsData, dnsHelpMsg = post_helper.handleTableQuery(dnsColumns, dnsOptions, nil, nil, dnsValid)

local supervision_support = proxy.get("uci.supervision.global.enabled")
local supervision_modes = {
  { "BFD", T"BFD" },
  { "DNS", T"DNS" },
  { "Disabled", T"Disabled" },
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
  supervision_modes_content = post_helper.handleQuery(supervision_modes_param, supervision_modes_valid)
end

---
--- TRIGGERED DURING POST OPERATION ON IP ADDRESS
---    1) GRABS POST ACTION AND ARGUMENTS FROM POST
---    2) CLEARS OUT MESSAGES FROM THE MESSAGE_HELPER QUEUE (THROUGH 'popMessages' CALL)
---    3) AFTER CHECKING WHICH PARAMETER IS GETTING UPDATED, VERIFIES FIELD VALUE
---    4) IF FIELD VALIDATION PASSES, TELL TRANSFORMER (THROUGH PROXY) TO PHYSICALLY UPDATE VALUE
---    5) UNDERLYING HARDWARE SHOULD GET CONFIGURED AUTOMATICALLY
if ngx.req.get_method() == "POST" then
  local postContent = ngx.req.get_post_args()
  local actionVal   = postContent["action"]
  local doit = false
  
  local ipVal       = postContent["wanIpAddrParam"]
  local netmaskVal  = postContent["wanNetmaskParam"]
  local gatewayVal  = postContent["wanGatewayParam"]    

  if actionVal == "wanIpAddrParam" then
      content["wanIpAddrParam"] = ipVal
      proxy.set(wanIpAddrPath_uci,ipVal)
      doit = true
  end
  if actionVal == "wanNetmaskParam" then
      content["wanNetmaskParam"] = netmaskVal
      proxy.set(wanNetmaskPath_uci,netmaskVal)
      doit = true
  end
  if actionVal == "wanGatewayParam" then
      content["wanGatewayParam"] = gatewayVal
      proxy.set(wanGatewayPath_uci,gatewayVal)
      doit = true
  end

  if content.uci_wan_ipv6 == "1" then
    local ipv4Auto = proxy.get("uci.network.interface.@wan.auto")[1].value
    local ipv6Auto = proxy.get("uci.network.interface.@wan6.auto")[1].value
    if ipv6Auto ~= ipv4Auto then
      proxy.set("uci.network.interface.@wan6.auto", ipv4Auto)
      doit = true
    end
  end

  if doit then
    proxy.apply()
  end
end
    
  ngx.print('\
');  
--- THIS SECTION SETS UP COSMETICS FOR THE SUB-MENU (MODAL)
--- EACH CONTROL HAS THE OPTION OF SETTING ATTRIBUTES (WRAPPED INTO A "CLASS" CONTAINER) 
--- IN THIS CASE, INPUT TEXT FIELDS ARE RESPONSIBLE FOR GATHERING INFORMATION ABOUT THE WAN INTERFACE
  ngx.print('\
');  
-- CLEAR ALL MESSAGES FROM THE QUEUE
ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  ngx.print('\
<fieldset>\
    <legend>');  ngx.print( T"Static Connection" ); ngx.print('</legend>\
    ');  
        local html = {}

        local wanIPClass = {}
        wanIPClass["input"] = {
            ["data-for"] = "wanIpAddrParam",
            ["pattern"] = ipv4pattern
        }

        local wanNetmaskClass = {}
        wanNetmaskClass["input"] = {
            ["data-for"] = "wanNetmaskParam",
            ["pattern"] = ipv4pattern
        }

        local wanGatewayClass = {}
        wanGatewayClass["input"] = {
            ["data-for"] = "wanGatewayParam",
            ["pattern"] = ipv4pattern
        }

        local switch_class_enable = {
          input = {
            class = "monitor-changes",
          }
        }
        local ipv6_input = {
          group = {
            class ="monitor-uci_wan_ipv6 monitor-1",
          },
        }
    
        -- UI CONTROLS LIST
        -- ARGUMENT 2 MUST MUST MATCH PARAMETER NAME FROM mapParams OR ERRORS WILL OCCUR DURING INTERFACE UPDATES
        html[#html+1] = ui_helper.createSwitch(T"Enabled", "uci_wan_auto", content["uci_wan_auto"])
        html[#html+1] = ui_helper.createSwitch(T"NAT", "uci_wan_masq", content.uci_wan_masq)
        html[#html+1] = ui_helper.createSwitch(T"IPv6", "uci_wan_ipv6", content.uci_wan_ipv6, switch_class_enable)
        html[#html+1] = ui_helper.createSimpleInputHidden("uci_wan6_proto", content.uci_wan6_proto)
        if supervision_support then
          html[#html+1] = ui_helper.createInputSelect(T"WAN Supervision", "supervision_mode", supervision_modes, supervision_modes_content["supervision_mode"])
          html[#html+1] = ui_helper.createSwitch(T"Supervision Enabled", "supervision_enabled", supervision_modes_content["supervision_enabled"])
        end
        html[#html+1] = ui_helper.createInputText(T"MAC Spoof (Wan MAC)","uci_wan_mac", content["uci_wan_mac"], nil, helpmsg["uci_wan_mac"])
        html[#html+1] = ui_helper.createInputText(T"IPv4 Address", "wanIpAddrParam", content["wanIpAddrParam"], wanIPClass, helpmsg["wanIpAddrParam"])
        html[#html+1] = ui_helper.createInputText(T"IPv4 Subnet Mask", "wanNetmaskParam", content["wanNetmaskParam"], wanNetmaskClass, helpmsg["wanNetmaskParam"])
        html[#html+1] = ui_helper.createInputText(T"IPv4 Gateway", "wanGatewayParam", content["wanGatewayParam"], wanGatewayClass, helpmsg["wanGatewayParam"])
        if dns_loopback ~= "" then
	        ajax_attr.span.id = "loopbackdns"
          html[#html+1] = ui_helper.createLabel(T"IPv4 DNS Servers", content_dns.loopbackdns,ajax_attr)
        end
        html[#html+1] = ui_helper.createInputText(T"IPv6 Address (CIDR)", "uci_wan6_addr", content["uci_wan6_addr"], ipv6_input, helpmsg["uci_wan6_addr"])
        html[#html+1] = ui_helper.createInputText(T"IPv6 Gateway", "uci_wan6_gw", content["uci_wan6_gw"], ipv6_input, helpmsg["uci_wan6_gw"])
        html[#html+1] = ui_helper.createInputText(T"IPv6 LAN Prefix (CIDR)", "uci_wan6_prefix", content["uci_wan6_prefix"], ipv6_input, helpmsg["uci_wan6_prefix"])

        ngx.print(html)
      ngx.print('\
</fieldset>\
');  
--- IN THIS SECTION, TABLE OF DNS SERVERS IS CREATED.  
--- HARD LIMITS ARE SET THROUGH "minEntries" AND "maxEntries" FIELDS OF THE "dnsOptions" TABLE
  ngx.print('\
<fieldset>\
    <legend>');  ngx.print( T"Domain Name Server Configuration"  ); ngx.print('</legend>\
    ');  
        local html2 = {}
        
        -- UI CONTROLS LIST
        html2[#html2+1] = ui_helper.createTable(dnsColumns, dnsData, dnsOptions, nil, dnsHelpMsg)        
        
        ngx.print(html2)
      ngx.print('\
</fieldset>    \
\
<script language ="javascript">\
$("#uci_wan_ipv6").change(function() {\
  if (this.value == "1") {\
    $("input[name=uci_wan6_proto]").val("static");\
  } else {\
    $("input[name=uci_wan6_proto]").val("dhcpv6");\
    $("#uci_wan6_addr").val("");\
    $("#uci_wan6_gw").val("");\
    $("#uci_wan6_prefix").val("");\
  }\
});\
$(document).ready(function(){\
  if ( $.trim($("#wanIpAddrParam").val()).length <=0 || $.trim($("#wanNetmaskParam").val()).length <=0 || $.trim($("#wanGatewayParam").val()).length <=0) {\
    $(\'#modal-no-change\').hide();\
    $(\'#modal-changes\').show();\
  }\
});\
</script>\
'); 