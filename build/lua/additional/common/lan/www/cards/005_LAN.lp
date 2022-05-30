--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local format = string.format
local session = ngx.ctx.session

if session:hasAccess("/modals/ethernet-modal.lp") then
  local dhcp4State_text = ""
  local dhcp4State
  local dhcp6State_text = ""
  local dhcp6State
  local slaacState_text = ""
  local slaacState
  local ipv6State_text

  local mapParams = {
    dhcpv4State = "uci.dhcp.dhcp.@lan.dhcpv4",
    ipv6State = "uci.network.interface.@lan.ipv6",
    dhcpv6State = "uci.dhcp.dhcp.@lan.dhcpv6",
    slaacState = "uci.dhcp.dhcp.@lan.ra",
    ignored = "uci.dhcp.dhcp.@lan.ignore",
    localdevIP = "rpc.network.interface.@lan.ipaddr",
    localdevmask = "uci.network.interface.@lan.netmask",
    ipv6prefix = "rpc.network.interface.@lan.ip6prefix_assignment",
  }
  content_helper.getExactContent(mapParams)

  if mapParams["ipv6State"] == "" or mapParams["ipv6State"] == "1" then
    if mapParams["dhcpv6State"] == "" or mapParams["dhcpv6State"] == "server" then
      if mapParams["ignored"] == "1" then
        dhcp6State_text = T"DHCPv6 ignored (Bridged mode)"
        dhcp6State = "2"
      else
        dhcp6State_text = T"DHCPv6 enabled"
        dhcp6State = "1"
      end
    else
      dhcp6State_text = T"DHCPv6 disabled"
      dhcp6State = "0"
    end
    if mapParams["slaacState"] == "" or mapParams["slaacState"] == "server" then
      if mapParams["ignored"] == "1" then
        slaacState_text = T"SLAAC + RA ignored (Bridged mode)"
        slaacState = "2"
      else
        slaacState_text = T"SLAAC + RA enabled"
        slaacState = "1"
      end
    else
      slaacState_text = T"SLAAC + RA disabled"
      slaacState = "0"
    end
  else
      dhcp6State_text = T"IPv6 disabled"
      dhcp6State = "0"
  end
  if mapParams["ipv6prefix"] == "" then
      ipv6State_text = T""
  else
      ipv6State_text = T"Prefix: "
  end

  if mapParams["dhcpv4State"] == "" or mapParams["dhcpv4State"] == "server" then
    if mapParams["ignored"] == "1" then
      dhcp4State_text = T"DHCPv4 ignored (Bridged mode)"
      dhcp4State = "2"
    else
      dhcp4State_text = T"DHCPv4 enabled"
      dhcp4State = "1"
    end
  else
    dhcp4State_text = T"DHCPv4 disabled"
    dhcp4State = "0"
    if dhcp6State == "0" and (not slaacState or slaacState == "0") then
      local adguard = proxy.get("rpc.gui.init.files.@AdGuardHome.active")
      if adguard and adguard[1].value == "1" then
        local fmt = T'<a style="color:inherit;text-decoration:none;" target="_blank" href="http://%s:8008"><b>AdGuard Home</b></a> %s enabled'
        dhcp4State = "1"
        dhcp4State_text = format(fmt,mapParams["localdevIP"],"DHCPv4")
        dhcp6State = "1"
        dhcp6State_text = format(fmt,mapParams["localdevIP"],"DHCPv6")
        slaacState = "1"
        slaacState_text = format(fmt,mapParams["localdevIP"],"SLAAC")
      end
    end
  end

  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader(T"Local Network","modals/ethernet-modal.lp"),'\
    <div class="content card_bg" data-bg-text="&#xf015;">\
      ');
      if require("bridgedmode_helper").isBridgedMode() then
        ngx.print(ui_helper.createSimpleLight("1","Bridged Mode"))
      end
      ngx.print(
        ui_helper.createSimpleLight(dhcp4State,dhcp4State_text),
        ui_helper.createSimpleLight(dhcp6State,dhcp6State_text)
      )
      if mapParams["ipv6State"] == "1" then
          ngx.print(ui_helper.createSimpleLight(slaacState,slaacState_text))
      end
      ngx.print(
        '<p class="subinfos">',
        format(T'LAN IP: <strong style="letter-spacing:-1px;font-size:12px;">%s</strong> <br>Subnet Mask: <strong style="letter-spacing:-1px;font-size:12px;">%s</strong>',mapParams["localdevIP"],mapParams["localdevmask"]),
        '<br>',
        format(T'%s <nobr><strong style="letter-spacing:-1px;font-size:12px;">%s</strong></nobr>',ipv6State_text,mapParams["ipv6prefix"]),
        '</p>\
    </div>\
  </div>\
</div>\
');
end
