--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local format = string.format
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local session = ngx.ctx.session

if session:hasAccess("/modals/ethernet-modal.lp") then
  local vlan_helper = require("ethernet-vlan_helper")
  local isBridgedMode = require("bridgedmode_helper").isBridgedMode()
  local dhcp_text = "DHCP: "
  local dhcp_state = "0"

  local lan = {
    ipv6State = "uci.network.interface.@lan.ipv6",
    dhcpv4State = "uci.dhcp.dhcp.@lan.dhcpv4",
    dhcpv6State = "uci.dhcp.dhcp.@lan.dhcpv6",
    slaacState = "uci.dhcp.dhcp.@lan.ra",
    ignored = "uci.dhcp.dhcp.@lan.ignore",
    ipv4addr = "rpc.network.interface.@lan.ipaddr",
    ipv6addr = "rpc.network.interface.@lan.ip6addr",
    ipv6prefix = "rpc.network.interface.@lan.ip6prefix_assignment",
    sleases = "uci.dhcp.hostNumberOfEntries",
  }
  if isBridgedMode and require("datamodel").get("rpc.network.interface.@lan6.ip6addr") then
    lan.ipv6State = "uci.network.interface.@lan6.ipv6"
    lan.ipv6addr = "rpc.network.interface.@lan6.ip6addr"
    lan.ipv6prefix = "rpc.network.interface.@lan6.ip6prefix_assignment"
  end
  content_helper.getExactContent(lan)

  local vlan_enabled = vlan_helper.get_vlan_enabled()
  local switch_vlan,_,sys_vlans = vlan_helper.get_switch_vlans(isBridgedMode)

  local adguard = {
    enabled = "rpc.gui.init.files.@AdGuardHome.active",
  }
  content_helper.getExactContent(adguard)

  local lanIPv6 = lan["ipv6State"] == "" or lan["ipv6State"] == "1"

  if not isBridgedMode then
    if adguard and (adguard.enabled == "1" and lan.dhcpv4State == "disabled" and lan.dhcpv6State == "disabled") then
      dhcp_state = "1"
      dhcp_text = format(T'<a style="color:inherit;text-decoration:none;" target="_blank" href="http://%s:8008"><b>AdGuard Home</b></a> DHCP enabled',lan["ipv4addr"])
    elseif lan["ipv6State"] == "" or lan["ipv6State"] == "1" then -- IPv6
      if lan["dhcpv4State"] ~= "disabled" then
        if lan["ignored"] == "1" then
          dhcp_state = "2"
          dhcp_text = dhcp_text.."v4 <i class='icon-ban-circle orange'></i>"
        else
          dhcp_state = "1"
          dhcp_text = dhcp_text.."v4 <i class='icon-ok-circle green'></i>"
        end
      else
        dhcp_state = "0"
        dhcp_text = dhcp_text.."v4 <i class='icon-remove-circle gray'></i>"
      end
      if lan["dhcpv6State"] ~= "disabled" then
        if lan["ignored"] == "1" then
          dhcp_state = dhcp_state ~= "1" and "2" or dhcp_state
          dhcp_text = dhcp_text.."v6 <i class='icon-ban-circle orange'></i>"
        else
          dhcp_state = dhcp_state ~= "1" and "1" or dhcp_state
          dhcp_text = dhcp_text.."v6 <i class='icon-ok-circle green'></i>"
        end
      else
        dhcp_state = dhcp_state ~= "1" and "0" or dhcp_state
        dhcp_text = dhcp_text.."v6 <i class='icon-remove-circle gray'></i>"
      end
      if lan["slaacState"] ~= "disabled" then
        if lan["ignored"] == "1" then
          dhcp_state = dhcp_state ~= "1" and "2" or dhcp_state
          dhcp_text = dhcp_text.."SLAAC+RA <i class='icon-ban-circle orange'></i>"
        else
          dhcp_state = dhcp_state ~= "1" and "1" or dhcp_state
          dhcp_text = dhcp_text.."SLAAC+RA <i class='icon-ok-circle green'></i>"
        end
      else
        dhcp_state = dhcp_state ~= "1" and "0" or dhcp_state
        dhcp_text = dhcp_text.."SLAAC+RA <i class='icon-remove-circle gray'></i>"
      end
    else --IPv4 only
      if lan["dhcpv4State"] ~= "disabled" then
        if lan["ignored"] == "1" then
          dhcp_state = "2"
          dhcp_text = "DHCP enabled but ignored"
        else
          dhcp_state = "1"
          dhcp_text = "DHCP enabled"
        end
      else
        if adguard and adguard.enabled == "1" then
          dhcp_state = "1"
          dhcp_text = format(T'<a style="color:inherit;text-decoration:none;" target="_blank" href="http://%s:8008"><b>AdGuard Home</b></a> DHCP enabled',lan["ipv4addr"])
        else
          dhcp_state = "0"
          dhcp_text = T"DHCP disabled"
        end
      end
    end
  end

  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader(T"Local Network","modals/ethernet-modal.lp",nil,nil,{icon={class="icon-home"}}),'\
    <div class="content card_bg" data-bg-text="&#xf015;">\
      ');
      if isBridgedMode then
        ngx.print(ui_helper.createSimpleLight("1","Bridged Mode"))
      end
      ngx.print(
        '<div class="light green"></div><p class="subinfos" style="line-height:14px">',
        format(T'Gateway IP: <strong style="letter-spacing:-1px;font-size:12px;">%s</strong>',lan["ipv4addr"]),
        '<br>'
      )
      if lanIPv6 then
        if lan["ipv6addr"] ~= "" then
          local addresses = require("split").split(untaint(lan["ipv6addr"])," ")
          local max_show = 2
          if isBridgedMode then
            max_show = 5
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
        if lan["ipv6prefix"] ~= "" then
          ngx.print(format(T'Prefix: <nobr><strong style="letter-spacing:-1px;font-size:12px;">%s</strong></nobr>',lan["ipv6prefix"]))
        end
      end
      ngx.print('</p>')
      if not isBridgedMode then
        ngx.print(ui_helper.createSimpleLight(dhcp_state,dhcp_text))
        if not adguard or adguard.enabled ~= "1" then
          ngx.print(
            '<span class="simple-desc"><i class="icon-pushpin status-icon" style="color:#69c469;"></i>',
              format('<strong>%d</strong><span class="modal-link" data-toggle="modal" data-remote="/modals/ethernet-static-leases-modal.lp" data-id="static-leases-modal"> Static %s defined</span>',lan.sleases,N('Lease','Leases',tonumber(lan.sleases))),
            '</span>')
        end
      end
      if vlan_enabled[1].value == "1" or sys_vlans > 0 then
        local desc = (#switch_vlan == sys_vlans) and "System " or ""
        ngx.print(
          '<span class="simple-desc"><i class="icon-code-fork status-icon" style="color:#69c469;"></i>',
            format('<strong>%d</strong><span class="modal-link" data-toggle="modal" data-remote="/modals/ethernet-vlan-modal.lp" data-id="vlan-modal"> %s%s active</span>',#switch_vlan,desc,N('VLAN','VLANs',#switch_vlan)),
          '</span>')
      else
        ngx.print(ui_helper.createSimpleLight("0",'<span class="modal-link" data-toggle="modal" data-remote="/modals/ethernet-vlan-modal.lp" data-id="vlan-modal">VLANs disabled</span>'))
      end
      ngx.print('\
    </div>\
  </div>\
</div>\
');
end
