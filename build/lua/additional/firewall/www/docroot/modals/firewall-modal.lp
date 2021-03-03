--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local message_helper = require("web.uimessage_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

if ngx.var.request_method == "POST" then
  local action = ngx.req.get_post_args()["action"]
  if action == "SAVE" or action == "TABLE-ADD" or action == "TABLE-MODIFY" or action == "TABLE-DELETE"  then
    proxy.set("rpc.gui.firewall.clearCache", "1")
  end
end

local incomingpolicy = {
  {"DROP", T"DROP"},
  {"REJECT", T"REJECT"},
}

local fw_levels = {
  {"lax", T"<strong>&nbsp;&nbsp;Low</strong>"},
  {"normal", T"<strong>&nbsp;&nbsp;Normal</strong>"},
  {"high", T"<strong>&nbsp;&nbsp;High</strong>"},
  {"user", T"<strong>&nbsp;&nbsp;User Defined</strong>"}
}

local fw_params = {
  fw_enable = "rpc.network.firewall.enable",
  fw_level = "rpc.network.firewall.mode",
  fw_incoming_policy = "rpc.network.firewall.userincomingdefault",
  fw_ping = "rpc.network.firewall.internetpingipv4",
  fw_ping_ipv6 = "rpc.network.firewall.internetpingipv6",
  fw_accept_redirects = "uci.firewall.defaults.@defaults[0].accept_redirects",
  fw_accept_source_route = "uci.firewall.defaults.@defaults[0].accept_source_route",
  fw_disable_ipv6 = "uci.firewall.defaults.@defaults[0].disable_ipv6",
  fw_drop_invalid = "uci.firewall.defaults.@defaults[0].drop_invalid",
  fw_syn_flood = "uci.firewall.defaults.@defaults[0].syn_flood",
  fw_synflood_burst = "uci.firewall.defaults.@defaults[0].synflood_burst",
  fw_synflood_rate = "uci.firewall.defaults.@defaults[0].synflood_rate",
  fw_tcp_ecn = "uci.firewall.defaults.@defaults[0].tcp_ecn",
  fw_tcp_syncookies = "uci.firewall.defaults.@defaults[0].tcp_syncookies",
  fw_tcp_window_scaling = "uci.firewall.defaults.@defaults[0].tcp_window_scaling",
}

local vB = post_helper.validateBoolean
local gVIES = post_helper.getValidateInEnumSelect
local vPN = post_helper.validatePositiveNum
local vNES = post_helper.validateNonEmptyString
local function vPktsPerSec(value, object, key)
  local packets = string.match(value, "%d+")
  if packets and packets ~= "" then
    object.fw_synflood_rate = packets.."/s"
    return true
  else
    return nil, "Invalid packets per second"
  end
end


local fw_valid = {
  fw_enable = vB,
  fw_level = gVIES(fw_levels),
  fw_incoming_policy = gVIES(incomingpolicy),
  fw_ping = vB,
  fw_ping_ipv6 = vB,
  fw_accept_redirects = vB,
  fw_accept_source_route = vB,
  fw_disable_ipv6 = vB,
  fw_drop_invalid = vB,
  fw_syn_flood = vB,
  fw_synflood_burst = vPN,
  fw_synflood_rate = vPktsPerSec,
  fw_tcp_ecn = vB,
  fw_tcp_syncookies = vB,
  fw_tcp_window_scaling = vB,
}

local content = post_helper.handleQuery(fw_params, fw_valid)
if content.fw_accept_redirects == "" then 
  content.fw_accept_redirects = "0"
end
if content.fw_accept_source_route == "" then 
  content.fw_accept_source_route = "0"
end
if content.fw_disable_ipv6 == "" then 
  content.fw_disable_ipv6 = "0"
end
if content.fw_drop_invalid == "" then 
  content.fw_drop_invalid = "0"
end
if content.fw_syn_flood == "" then 
  content.fw_syn_flood = "0"
end
if content.fw_synflood_burst == "" then 
  content.fw_synflood_burst = "50"
end
if content.fw_synflood_rate == "" then 
  content.fw_synflood_rate = "25"
else
  content.fw_synflood_rate = string.match(content.fw_synflood_rate, "%d+")
end
if content.fw_tcp_ecn == "" then 
  content.fw_tcp_ecn = "0"
end
if content.fw_tcp_syncookies == "" then 
  content.fw_tcp_syncookies = "1"
end
if content.fw_tcp_window_scaling == "" then 
  content.fw_tcp_window_scaling = "1"
end


ngx.print(ui_helper.createHeader(T"Firewall", true, false))   ngx.print('\
<div class="modal-body update">\
  '); lp.include("tabs-firewall.lp") ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-modal.lp">\
    <fieldset>\
      '); 
      ngx.print(ui_helper.createMessages(message_helper.popMessages()))
      ngx.print('<legend>');  ngx.print( T"Firewall Configuration" ); ngx.print('</legend>');

      local fwenable_attr = {
        input = {
          class = "monitor-changes"
        }
      }
      ngx.print(ui_helper.createSwitch(T"Enabled", "fw_enable", content["fw_enable"], fwenable_attr)) 
      
      local enable_attr = { 
        alert = { 
          class = "alert-danger monitor-fw_enable monitor-0" 
        }, 
      }
      ngx.print(ui_helper.createAlertBlock(T("WARNING: Setting Enabled to OFF causes the firewall to ACCEPT all incoming connections from the WAN!<br>NOTE: You can only disable the firewall when the current level is <strong>Normal</strong>."), enable_attr)); ngx.print('\
      ');
      local html = {}

      local fwlevel_attr = {
        select = {
          class = "monitor-changes"
        }
      }
      html[#html+1] = ui_helper.createInputSelect(T"Level", "fw_level", fw_levels, content["fw_level"], fwlevel_attr)

      local lax_attr = {
        alert = {
          class = "alert-info monitor-fw_level monitor-lax"
        },
      }
      html[#html+1] = ui_helper.createAlertBlock(T("In <strong>Low mode</strong>, the firewall will allow all outgoing connections. Unless the firewall is disabled, it will reject unknown incoming connections but acknowledge the presence of the firewall."), lax_attr)

      local normal_attr = {
        alert = {
          class = "alert-info monitor-fw_level monitor-normal"
        },
      }
      html[#html+1] = ui_helper.createAlertBlock(T"In <strong>Normal mode</strong>, the firewall will allow all outgoing connections. Unless the firewall is disabled, it will silently drop unknown incoming connections.", normal_attr)

      local high_attr = {
        alert = {
          class = "alert-info monitor-fw_level monitor-high"
        },
      }
      html[#html+1] = ui_helper.createAlertBlock(T"In <strong>High mode</strong>, the firewall will allow outgoing connections to the following services: HTTP, HTTPS, SMTP, POP3, IMAP, SSH. Unless the firewall is disabled, it will silently drop unknown incoming connections. This may impact on web services like Internet Speed Tests.", high_attr)

      local user_attr = {
        alert = {
          class = "alert-info monitor-fw_level monitor-user"
        },
      }
      html[#html+1] = ui_helper.createAlertBlock(T"In <strong>User Defined mode</strong>, you can configure individual rules for the firewall as well as the default behavior.", user_attr)

      local outgoing_attr = {
        group = {
            class = "monitor-fw_level monitor-user"
        }
      }

      local info_attr = {
          alert = {
              class = "alert-info monitor-fw_level monitor-user"
          },
      }

      local number_attr = {
        input = {
          type = "number",
          min = "0",
          style = "width:100px",
        }
      }
  
      html[#html+1] = ui_helper.createInputSelect(T"Incoming default policy", "fw_incoming_policy", incomingpolicy, content["fw_incoming_policy"], outgoing_attr)
      html[#html+1] = ui_helper.createAlertBlock(T("The <strong>incoming policy</strong> defines what is done with packets destined to the firewall. They can be either REJECTED (the firewall will notify the sender they were rejected) or DROPPED (the firewall will silently discard those packets)."), info_attr)
      html[#html+1] = ui_helper.createSwitch(T"Answer IPv4 ping", "fw_ping", content["fw_ping"])
      html[#html+1] = ui_helper.createSwitch(T"Answer IPv6 ping", "fw_ping_ipv6", content["fw_ping_ipv6"])

      html[#html+1] = '<legend><small>Advanced Settings</small></legend>'
      html[#html+1] = ui_helper.createSwitch(T"Drop invalid packets", "fw_drop_invalid", content["fw_drop_invalid"])
      html[#html+1] = ui_helper.createSwitch(T"SYN flood protection", "fw_syn_flood", content["fw_syn_flood"])
      html[#html+1] = ui_helper.createInputText(T"SYN flood rate limit (pkts/sec)", "fw_synflood_rate", content["fw_synflood_rate"], number_attr)
      html[#html+1] = ui_helper.createInputText(T"SYN flood burst limit", "fw_synflood_burst", content["fw_synflood_burst"], number_attr)
      html[#html+1] = ui_helper.createSwitch(T"TCP SYN cookies", "fw_tcp_syncookies", content["fw_tcp_syncookies"])
      html[#html+1] = ui_helper.createSwitch(T"TCP ECN", "fw_tcp_ecn", content["fw_tcp_ecn"])
      html[#html+1] = ui_helper.createSwitch(T"TCP Window Scaling", "fw_tcp_window_scaling", content["fw_tcp_window_scaling"])
      html[#html+1] = ui_helper.createSwitch(T"Accept redirects", "fw_accept_redirects", content["fw_accept_redirects"])
      html[#html+1] = ui_helper.createSwitch(T"Accept source route", "fw_accept_source_route", content["fw_accept_source_route"])
      html[#html+1] = ui_helper.createSwitch(T"Disable IPv6 rules", "fw_disable_ipv6", content["fw_disable_ipv6"])
      ngx.print(html)

      ngx.print('\
    </fieldset>\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter());
