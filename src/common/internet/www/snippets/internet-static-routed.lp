--pretranslated: do not change this file

gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local message_helper = require("web.uimessage_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local gAV = post_helper.getAndValidation
local gOV = post_helper.getOptionalValidation
local vNES = post_helper.validateNonEmptyString
local nEIPV = post_helper.getAndValidation(vNES,post_helper.advancedIPValidation)
local vIAS6 = gOV(post_helper.validateIPAndSubnet(6))
local vSIIP6 = gOV(post_helper.validateStringIsIPv6)
local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean
local rIPV = post_helper.reservedIPValidation

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
  local valid,msg = vB(value)
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

local function validateIPv6(value,object,_)
  local valid,msg = vB(value)
  if valid and value == "1" then
    if object.uci_wan6_addr == "" or object.uci_wan6_gw == "" or object.uci_wan6_prefix == "" then
      return nil,"IPv6 Address, Gateway and LAN Prefix are required"
    end
  end
  return valid,msg
end

local wanIpAddrPath_uci   = "uci.network.interface.@wan.ipaddr"
local wanNetmaskPath_uci  = "uci.network.interface.@wan.netmask"
local wanGatewayPath_uci  = "uci.network.interface.@wan.gateway"

local mapParams = {
  wanIpAddrParam    = wanIpAddrPath_uci,
  wanNetmaskParam   = wanNetmaskPath_uci,
  wanGatewayParam   = wanGatewayPath_uci,
  uci_wan_auto = "uci.network.interface.@wan.auto",
  uci_wan_masq = "uci.firewall.zone."..zonenum..".masq",
  uci_wan_mac = "uci.network.interface.@wan.macaddr",
	uci_wan6_auto = "uci.network.interface.@wan6.auto",
  uci_wan6_proto = "uci.network.interface.@wan6.proto",
  uci_wan6_addr = "uci.network.interface.@wan6.ip6addr",
  uci_wan6_gw = "uci.network.interface.@wan6.ip6gw",
  uci_wan6_prefix = "uci.network.interface.@wan6.ip6prefix",
}

local ipv6_proto_map = {
  { "static",T"Static" },
  { "dhcpv6",T"DHCPv6/Router Advertisement" },
}

local mapValid = {
  wanIpAddrParam    = gAV(nEIPV,rIPV),
  wanNetmaskParam   = post_helper.validateIPv4Netmask,
  wanGatewayParam   = gAV(nEIPV,rIPV),
  uci_wan_auto = vB,
  uci_wan_masq = validateNat,
  uci_wan_mac = gOV(post_helper.validateStringIsMAC),
  uci_wan6_proto = gVIES(ipv6_proto_map),
  uci_wan6_auto = validateIPv6,
  uci_wan6_addr = vIAS6,
  uci_wan6_gw = vSIIP6,
  uci_wan6_prefix = vIAS6,
}

local content,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if not helpmsg then helpmsg = {} end

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
    supervision_mode = gVIES(supervision_modes),
    supervision_enabled = vB,
  }
  supervision_modes_content = post_helper.handleQuery(supervision_modes_param,supervision_modes_valid)
end

if content.wanIpAddrParam == "" then
  content.wanIpAddrParam = proxy.get("rpc.network.interface.@wan.ipaddr")[1].value
  content.wanNetmaskParam = post_helper.cidr2mask(untaint(proxy.get("rpc.network.interface.@wan.ipmask")[1].value))
  content.wanGatewayParam = proxy.get("rpc.network.interface.@wan.nexthop")[1].value
end
if content.uci_wan6_addr == "" then
  local ipv6addr = proxy.get("rpc.network.interface.@wan6.ipv6uniqueglobaladdr","rpc.network.interface.@wan6.ipv6uniquelocaladdr")
  if ipv6addr then
    if ipv6addr[1].value ~= "" then
      content.uci_wan6_addr = ipv6addr[1].value
    else
      content.uci_wan6_addr = ipv6addr[2].value
    end
    content.uci_wan6_gw = proxy.get("rpc.network.interface.@wan6.nexthop")[1].value
    content.uci_wan6_prefix = proxy.get("rpc.network.interface.@wan6.ip6prefix")[1].value
    content.uci_wan6_auto = "0"
  end
end

if content.uci_wan_auto ~= "0" then
  content.uci_wan_auto = "1"
end
if content.uci_wan6_auto ~= "0" then
  content.uci_wan6_auto = "1"
end

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

  local wan_ipv6 = proxy.get("uci.network.interface.@wan.ipv6")[1].value
  if wan_ipv6 ~= content.uci_wan6_auto then
    proxy.set("uci.network.interface.@wan.ipv6",content.uci_wan6_auto)
    doit = true
  end

  if content.uci_wan6_auto ~= "0" then
    local ipv4ifname = proxy.get("uci.network.interface.@wan.ifname")[1].value
    local ipv6ifname = proxy.get("uci.network.interface.@wan6.ifname")[1].value
    local ipv4proto = proxy.get("uci.network.interface.@wan.proto")[1].value
    local ipv6proto = proxy.get("uci.network.interface.@wan6.proto")[1].value

    if ipv6ifname ~= ipv4ifname then
      proxy.set("uci.network.interface.@wan6.ifname",ipv4ifname)
      doit = true
    end
    if ipv6proto ~= ipv4proto then
      proxy.set("uci.network.interface.@wan6.proto",ipv4proto)
      doit = true
    end
  end

  local wsens = proxy.get("uci.wansensing.global.enable")
  if wsens and wsens[1].value == "1" then
    proxy.get("uci.wansensing.global.enable","0")
    doit = true
  end

  if doit then
    proxy.apply()
  end
end

  ngx.print('\
');
  ngx.print('\
');
ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  ngx.print('\
<fieldset>\
  <legend>');  ngx.print( T"Static Connection" ); ngx.print('</legend>\
  ');
  local html = {}

  local ipv4pattern = "^(((([1]?\\d)?\\d|2[0-4]\\d|25[0-5])\\.){3}(([1]?\\d)?\\d|2[0-4]\\d|25[0-5]))$"

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
      class ="monitor-uci_wan6_auto monitor-1",
    },
  }

  html[#html+1] = ui_helper.createSwitch(T"IPv4 Enabled on Boot","uci_wan_auto",content.uci_wan_auto,nil,helpmsg["uci_wan_auto"])
  html[#html+1] = ui_helper.createSwitch(T"NAT","uci_wan_masq",content.uci_wan_masq,nil,helpmsg["uci_wan_masq"])
  html[#html+1] = ui_helper.createInputText(T"IPv4 Address","wanIpAddrParam",content["wanIpAddrParam"],wanIPClass,helpmsg["wanIpAddrParam"])
  html[#html+1] = ui_helper.createInputText(T"IPv4 Subnet Mask","wanNetmaskParam",content["wanNetmaskParam"],wanNetmaskClass,helpmsg["wanNetmaskParam"])
  html[#html+1] = ui_helper.createInputText(T"IPv4 Gateway","wanGatewayParam",content["wanGatewayParam"],wanGatewayClass,helpmsg["wanGatewayParam"])
  html[#html+1] = ui_helper.createSwitch(T"IPv6 Enabled on Boot","uci_wan6_auto",content.uci_wan6_auto,switch_class_enable,helpmsg["uci_wan6_auto"])
  html[#html+1] = ui_helper.createSimpleInputHidden("uci_wan6_proto",content.uci_wan6_proto)
  html[#html+1] = ui_helper.createInputText(T"IPv6 Address<span class='icon-question-sign' title='Include the CIDR suffix (e.g. /128)'></span>","uci_wan6_addr",content["uci_wan6_addr"],ipv6_input,helpmsg["uci_wan6_addr"])
  html[#html+1] = ui_helper.createInputText(T"IPv6 Gateway","uci_wan6_gw",content["uci_wan6_gw"],ipv6_input,helpmsg["uci_wan6_gw"])
  html[#html+1] = ui_helper.createInputText(T"IPv6 LAN Prefix<span class='icon-question-sign' title='Include the CIDR suffix (e.g. /64)'></span>","uci_wan6_prefix",content["uci_wan6_prefix"],ipv6_input,helpmsg["uci_wan6_prefix"])
  if supervision_support then
    html[#html+1] = ui_helper.createInputSelect(T"WAN Supervision","supervision_mode",supervision_modes,supervision_modes_content["supervision_mode"])
    html[#html+1] = ui_helper.createSwitch(T"Supervision Enabled","supervision_enabled",supervision_modes_content["supervision_enabled"])
  end
  html[#html+1] = ui_helper.createInputText(T"MAC Spoof (Wan MAC)","uci_wan_mac",content["uci_wan_mac"],nil,helpmsg["uci_wan_mac"])

  ngx.print(html)
  ngx.print('\
</fieldset>\
');
  ngx.print('\
<script language ="javascript">\
  $("#uci_wan6_auto").change(function() {\
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