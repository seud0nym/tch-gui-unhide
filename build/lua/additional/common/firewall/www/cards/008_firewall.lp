--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
if session:hasAccess("/modals/firewall-modal.lp") then
  local proxy = require("datamodel")
  local untaint_mt = require("web.taint").untaint_mt
  local ui_helper = require("web.ui_helper")
  local content_helper = require("web.content_helper")

  local format = string.format
  ---@diagnostic disable-next-line: undefined-field
  local untaint = string.untaint

  local content = {
    mode = "rpc.network.firewall.mode",
    fwd_ipv4_rules = "uci.firewall.userredirectNumberOfEntries",
    fwd_ipv6_rules = "uci.firewall.pinholeruleNumberOfEntries",
    lanIpv6Enabled = "uci.network.interface.@lan.ipv6",
    pinholeEnabled = "uci.firewall.rulesgroup.@pinholerules.enabled",
  }

  content_helper.getExactContent(content)
  local fw_incoming_policy
  local zones = content_helper.convertResultToObject("uci.firewall.zone.",proxy.get("uci.firewall.zone."))
  for _,zone in ipairs(zones) do
    if zone.wan == "1" then
      fw_incoming_policy = untaint(zone.input)
      break
    end
  end

  local fw_status_light_map = {
    DROP = "1",
    REJECT = "2",
    ACCEPT = "4"
  }

  local modes = setmetatable({
    lax = T"Low",
    normal = T"Normal",
    high = T"High",
    user = T"User Defined"
  },untaint_mt)

  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeader(T"Firewall","modals/firewall-modal.lp") ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf132;">\
    ');
    local fw_status = format("Default Incoming Policy: <strong>%s</strong>",fw_incoming_policy)
    ngx.print(ui_helper.createSimpleLight(fw_status_light_map[fw_incoming_policy],fw_status))
    ngx.print('\
      <p class="subinfos">\
        ');
        local html = {}
        html[#html+1] = format(T"Firewall level: <strong>%s</strong><br>",modes[content.mode] or "")

        local fwd_modal_link = "class=\"modal-link\" data-toggle=\"modal\" data-remote=\"/modals/firewall-port-forwarding-modal.lp\" data-id=\"firewall-port-forwarding-modal\""
        local fwd_count = tonumber(content.fwd_ipv4_rules)
        if content.lanIpv6Enabled ~= "0" and content.pinholeEnabled == "1" then
          fwd_count = fwd_count + tonumber(content.fwd_ipv6_rules)
        end
        html[#html+1] = format(N("<strong %1$s>%2$d Port Forwarding rule</strong> defined","<strong %1$s>%2$d Port Forwarding rules</strong> defined",fwd_count),fwd_modal_link,fwd_count)
        html[#html+1] = "<br>"

        local nat_alg_card_hidden = proxy.get("uci.web.card.@card_natalghelper.hide")
        if not nat_alg_card_hidden or nat_alg_card_hidden[1].value == "1" then
          local alg_modal_link = "class=\"modal-link\" data-toggle=\"modal\" data-remote=\"/modals/nat-alg-helper-modal.lp\" data-id=\"nat-alg-helper-modal\""
          local enabled_count = 0
          local disabled_count = 0
          local helper_uci_path = "uci.firewall.helper."
          local helper_uci_content = proxy.get(helper_uci_path)
          helper_uci_content = content_helper.convertResultToObject(helper_uci_path,helper_uci_content)
          for _,v in ipairs(helper_uci_content) do
            if v.intf ~= "loopback" then
              if v.enable ~= "0" then
                enabled_count = enabled_count + 1
                else
                disabled_count = disabled_count + 1
              end
            end
          end
          if enabled_count > 0 then
            html[#html+1] = format(N("<strong %1$s>%2$d NAT Helper</strong> enabled","<strong %1$s>%2$d NAT Helpers</strong> enabled",enabled_count),alg_modal_link,enabled_count)
          else
            html[#html+1] = format(N("<strong %1$s>%2$d NAT Helper</strong> disabled","<strong %1$s>%2$d NAT Helpers</strong> disabled",disabled_count),alg_modal_link,disabled_count)
          end
          html[#html+1] = "<br>"
        end

        local dosprotect = proxy.get("uci.dosprotect.globals.enabled")
        if dosprotect and dosprotect[1].value ~="" then
          local dosprotect = dosprotect[1].value or "0"
          local dosprotect_status
          if dosprotect and dosprotect == "1" then
            dosprotect_status = T"DoS Protect enabled"
          else
            dosprotect_status = T"DoS Protect disabled"
          end
          html[#html+1] = ui_helper.createSimpleLight(dosprotect,dosprotect_status)
          html[#html+1] = "<br>"
        end

        local dns6_int_state = proxy.get("uci.tproxy.rule.@dnsv6.enabled")
        local dns4_int_state = "0"
        local dns_int_modal_link = "class=\"modal-link\" data-toggle=\"modal\" data-remote=\"/modals/firewall-dns_int-modal.lp\" data-id=\"firewall-dns_int-modal\""
        for _,v in ipairs(proxy.getPN("uci.firewall.redirect.",true)) do
          local path = v.path
          local values = proxy.get(path.."name",path.."enabled")
          if values then
            local name = values[1].value
            if name == "Redirect-DNS" or name == "Intercept-DNS" then
              if values[2] and values[2].value ~= "0" then
                dns4_int_state = "1"
              end
              break
            end
          end
        end
        if dns6_int_state then
          dns6_int_state = dns6_int_state[1].value
        end
        local dns_int_state, dns_int_text, dns_int_family
        if dns4_int_state == "1" or dns6_int_state == "1" then
          dns_int_state = "1"
          dns_int_text = "enabled"
          if dns4_int_state == "1" and dns6_int_state == "1" then
            dns_int_family = ""
          elseif dns4_int_state == "1" then
            dns_int_family = "IPv4"
          else
            dns_int_family = "IPv6"
          end
        else
          dns_int_state = "0"
          dns_int_text = "disabled"
          dns_int_family = ""
        end

        html[#html+1] = ui_helper.createSimpleLight(dns_int_state,T(format("<span %s>%s DNS Intercept %s</span>",dns_int_modal_link,dns_int_family,dns_int_text)))
        html[#html+1] = "<br>"

        ngx.print(html)
        ngx.print('\
      </p>\
    </div>\
  </div>\
</div>\
');
end
  ngx.print('\
');