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
    ip_enabled = "uci.intrusion_protect.config.enabled",
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
  <div class="smallcard">',
    ui_helper.createCardHeader(T"Firewall","modals/firewall-modal.lp",nil,nil,{icon={class="icon-shield"}}),'\
    <div class="content card_bg" data-bg-text="&#xf132;">\
    ');
    local fw_status = format("Default Incoming Policy: <strong>%s</strong>",fw_incoming_policy)
    ngx.print(ui_helper.createSimpleLight(fw_status_light_map[fw_incoming_policy],fw_status))
    ngx.print('\
      <p class="subinfos">\
        ');
        local html = {}
        html[#html+1] = format(T"Firewall level: <strong>%s</strong><br>",modes[content.mode] or "")

        local fwd_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/firewall-port-forwarding-modal.lp" data-id="firewall-port-forwarding-modal"'
        local fwd_count = tonumber(content.fwd_ipv4_rules)
        if content.lanIpv6Enabled ~= "0" and content.pinholeEnabled == "1" then
          fwd_count = fwd_count + tonumber(content.fwd_ipv6_rules)
        end
        html[#html+1] = format(N("<strong %1$s>%2$d Port Forward rule</strong> defined","<strong %1$s>%2$d Port Forward rules</strong> defined",fwd_count),fwd_modal_link,fwd_count)
        html[#html+1] = "<br>"

        local nat_alg_card_hidden = proxy.get("uci.web.card.@card_natalghelper.hide")
        if not nat_alg_card_hidden or nat_alg_card_hidden[1].value == "1" then
          local alg_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/nat-alg-helper-modal.lp" data-id="nat-alg-helper-modal"'
          local enabled_count = 0
          local helper_uci_path = "uci.firewall.helper."
          local helper_uci_content = proxy.get(helper_uci_path)
          helper_uci_content = content_helper.convertResultToObject(helper_uci_path,helper_uci_content)
          for _,v in ipairs(helper_uci_content) do
            if v.intf ~= "loopback" then
              if v.enable ~= "0" then
                enabled_count = enabled_count + 1
              end
            end
          end
          html[#html+1] = T(format(N("<strong %1$s>%2$d NAT Helper</strong> enabled","<strong %1$s>%2$d NAT Helpers</strong> enabled",enabled_count),alg_modal_link,enabled_count))
          html[#html+1] = "<br>"
        end

        local dosprotect = proxy.get("uci.dosprotect.globals.enabled")
        if dosprotect and dosprotect[1].value ~="" then
          local dosprotect_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/dosprotect-modal.lp" data-id="dosprotect-modal"'
          local dosprotect_status
          dosprotect = dosprotect[1].value or "0"
          if dosprotect and dosprotect == "1" then
            dosprotect_status = "enabled"
          else
            dosprotect_status = "disabled"
          end
          html[#html+1] = ui_helper.createSimpleLight(dosprotect,T(format("<span %s>DoS Protection %s</span>",dosprotect_modal_link,dosprotect_status)))
          html[#html+1] = "<br>"
        end

        local ip_modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/firewall-intrusion_protect-modal.lp" data-id="intrusion_protect-modal"'
        local ip_status
        if content["ip_enabled"] == "1" then
          ip_status = "enabled"
        else
          content["ip_enabled"] = "0"
          ip_status = "disabled"
        end
        html[#html+1] = ui_helper.createSimpleLight(content["ip_enabled"],T(format("<span %s>Intrusion Protection %s</span>",ip_modal_link,ip_status)))
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