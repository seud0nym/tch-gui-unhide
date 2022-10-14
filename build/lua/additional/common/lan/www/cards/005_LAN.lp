--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local format = string.format
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local session = ngx.ctx.session

if session:hasAccess("/modals/ethernet-modal.lp") then
  local isBridgedMode = require("bridgedmode_helper").isBridgedMode()
  local dhcp4State_text = ""
  local dhcp4State
  local dhcp6State_text = ""
  local dhcp6State
  local slaacState_text = ""
  local slaacState

  local lanParams = {
    ipv6State = "uci.network.interface.@lan.ipv6",
    dhcpv4State = "uci.dhcp.dhcp.@lan.dhcpv4",
    dhcpv6State = "uci.dhcp.dhcp.@lan.dhcpv6",
    slaacState = "uci.dhcp.dhcp.@lan.ra",
    ignored = "uci.dhcp.dhcp.@lan.ignore",
    ipv4addr = "rpc.network.interface.@lan.ipaddr",
    ipv6addr = "rpc.network.interface.@lan.ip6addr",
    ipv6prefix = "rpc.network.interface.@lan.ip6prefix_assignment",
  }
  if isBridgedMode and require("datamodel").get("rpc.network.interface.@lan6.ip6addr") then
    lanParams.ipv6addr = "rpc.network.interface.@lan6.ip6addr"
  end
  content_helper.getExactContent(lanParams)

  local lanIPv6 = lanParams["ipv6State"] == "" or lanParams["ipv6State"] == "1"

  if not isBridgedMode then
    if lanIPv6 then
      if lanParams["dhcpv6State"] == "" or lanParams["dhcpv6State"] == "server" then
        if lanParams["ignored"] == "1" then
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
      if lanParams["slaacState"] == "" or lanParams["slaacState"] == "server" then
        if lanParams["ignored"] == "1" then
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
    end

    if lanParams["dhcpv4State"] == "" or lanParams["dhcpv4State"] == "server" then
      if lanParams["ignored"] == "1" then
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
          dhcp4State_text = format(fmt,lanParams["ipv4addr"],"DHCPv4")
          dhcp6State = "1"
          dhcp6State_text = format(fmt,lanParams["ipv4addr"],"DHCPv6")
          slaacState = "1"
          slaacState_text = format(fmt,lanParams["ipv4addr"],"SLAAC")
        end
      end
    end
  end

  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader(T"Local Network","modals/ethernet-modal.lp"),'\
    <div class="content card_bg" data-bg-text="&#xf015;">\
      ');
      if isBridgedMode then
        ngx.print(ui_helper.createSimpleLight("1","Bridged Mode"))
      else
        ngx.print(ui_helper.createSimpleLight(dhcp4State,dhcp4State_text))
        if lanIPv6 then
          ngx.print(
            ui_helper.createSimpleLight(dhcp6State,dhcp6State_text),
            ui_helper.createSimpleLight(slaacState,slaacState_text)
          )
        end
      end
      ngx.print(
        '<p class="subinfos" style="line-height:14px">',
        format(T'Gateway IP: <strong style="letter-spacing:-1px;font-size:12px;">%s</strong>',lanParams["ipv4addr"]),
        '<br>'
      )
      if lanIPv6 then
        if lanParams["ipv6addr"] ~= "" then
          local addresses = require("split").split(untaint(lanParams["ipv6addr"])," ")
          local max_show = 2
          if isBridgedMode then
            max_show = 7
          end
          ngx.print(T'<strong style="letter-spacing:-1px;font-size:12px;">')
          for i,address in ipairs(addresses) do
            if i > 1 and i <= max_show then
              ngx.print('<br>')
            end
            if i <= max_show then
              ngx.print(address)
            end
          end
          if #addresses > max_show then
            ngx.print("<small> + ",(#addresses - max_show)," more</small>")
          end
          ngx.print("<br>")
        end
        ngx.print(T'</strong>')
        if lanParams["ipv6prefix"] ~= "" then
          ngx.print(format(T'Prefix: <nobr><strong style="letter-spacing:-1px;font-size:12px;">%s</strong></nobr>',lanParams["ipv6prefix"]))
        end
      end
      ngx.print(
      '</p>\
    </div>\
  </div>\
</div>\
');
end
