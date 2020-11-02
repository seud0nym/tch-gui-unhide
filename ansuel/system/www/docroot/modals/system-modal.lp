--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ngx, string = ngx, string
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local format, match = string.format, string.match
local untaint = string.untaint

local gOV = post_helper.getOptionalValidation
local gVIES = post_helper.getValidateInEnumSelect
local gVIPIL = post_helper.getValidationIfPropInList
local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString
local vSII = post_helper.validateStringIsIP
local vSIP = post_helper.validateStringIsPort
local vSIPR = post_helper.validateStringIsPortRange

local syslog_present, syslog_filter_sw = "0", "0"
local Syslog = proxy.get("uci.system.system.@system[0].log_ip","uci.system.system.@system[0].log_filter_ip")
if Syslog[1].value ~= "" or Syslog[2].value ~= "" then
  syslog_present = "1"
  if Syslog[2].value ~= "" then
    syslog_filter_sw = "1"
  end
end

local current_theme = content_helper.readfile("/usr/tch-gui-unhide.theme","line")
local theme_settings = {}
for k, v in string.gmatch(current_theme, "(%w+)=(%w+)") do
  theme_settings[k] = string.untaint(v)
end
local theme_names = {
    {"light", T"Light"},
    {"night", T"Night"},
    {"telstra", T"Telstra Branding"},
}
local theme_colours = {
    {"blue", T"Blue"},
    {"green", T"Green"},
    {"orange", T"Orange"},
    {"purple", T"Purple"},
    {"red", T"Red"},
    {"monochrome", T"Monochrome"},
}
local card_icons_options = {
    {"visible", T"Visible"},
    {"hidden", T"Hidden"},
}

if ngx.var.request_method == "POST" then
  local vars = ngx.req.get_post_args()
  if vars["action"] == "SAVE" then
    local theme_name = string.untaint(vars["theme_name"])
    local theme_colour = string.untaint(vars["theme_colour"])
    local card_icons = string.untaint(vars["card_icons"])
    if theme_name ~= theme_settings["THEME"] or theme_colour ~= theme_settings["COLOR"] or card_icons ~= theme_settings["ICONS"] then
      theme_settings["THEME"] = theme_name
      theme_settings["COLOR"] = theme_colour
      theme_settings["ICONS"] = card_icons
      local file, err_msg = io.open("/usr/tch-gui-unhide.theme", "w")
      if file then
        file:write("THEME=" .. theme_settings["THEME"] .. "; COLOR=" .. theme_settings["COLOR"] .. "; ICONS=" .. theme_settings["ICONS"] .. "; KEEPLP=" .. theme_settings["KEEPLP"] .. ";")
        file:close()
        message_helper.pushMessage("Theme change will be visible when you close this window.", "info")
      else
        message_helper.pushMessage("Faied to persist theme preferences: " .. err_msg, "error")
      end
    end

    syslog_present = vars["syslog_present"]
    syslog_filter_sw = vars["syslog_filter_sw"]
    proxy.set("uci.system.system.@system[0].log_ip","")
    proxy.set("uci.system.system.@system[0].log_filter_ip","")
    proxy.apply()
  end
end

local function validateFilter(value, object, key)
  return true
end

local filter_columns = {
  {
    header = T"Filter",
    name = "filter_string",
    param = "value",
    type = "text",
  }
}
local filter_valid = {
  filter_string = validateFilter
}
local filter_options = {
  tableid = "syslog_filters",
  basepath = "uci.system.system.@system[0].log_filter.@.",
  createMsg = T"Add new Syslog Filter",
  minEntries = 1,
  maxEntries = 10,
}
local filter_data, filter_helpmsg = post_helper.handleTableQuery(filter_columns, filter_options, nil, nil, filter_valid)

local syslog_params = {
  syslog_filter_ip = "uci.system.system.@system[0].log_filter_ip",
  syslog_port = "uci.system.system.@system[0].log_port",
  syslog_ip = "uci.system.system.@system[0].log_ip",
}
local syslog_valid = {
    syslog_present = vB,
    syslog_filter_sw = vB,
    syslog_ip = gVIPIL(gVIPIL(gOV(vSII), "syslog_filter_sw", {"0"}), "syslog_present", {"1"}),
    syslog_filter_ip = gVIPIL(gVIPIL(gOV(vSII), "syslog_filter_sw", {"1"}), "syslog_present", {"1"}),
    syslog_port = gVIPIL(vSIPR, "syslog_present", {"1"}),
    post_helper.validateStringIsPortRange
}
local syslog_params, syslog_helpmsg = post_helper.handleQuery(syslog_params, syslog_valid)

local authentication_choices = {
  {"on", T"Certificate or Password"},
  {"off", T"Certificate (Recommended)"},
}

local ssh_wan_enable = untaint(proxy.get("uci.dropbear.dropbear.@wan.enable")[1].value)
local ssh_wan_port = untaint(proxy.get("uci.dropbear.dropbear.@wan.Port")[1].value)
local ssh_wan_src_ip = untaint(proxy.get("uci.dropbear.dropbear.@wan.AllowedClientIPs")[1].value)
ngx.log(ngx.ALERT, "SSH @wan.enable = "..ssh_wan_enable.." ssh_wan_port = "..ssh_wan_port)
local ssh_params = {
  ssh_lan_enable = "uci.dropbear.dropbear.@lan.enable",
  ssh_lan_pwd_auth = "uci.dropbear.dropbear.@lan.PasswordAuth",
  ssh_lan_root_pwd_auth = "uci.dropbear.dropbear.@lan.RootPasswordAuth",
  ssh_wan_enable = "uci.dropbear.dropbear.@wan.enable",
  ssh_wan_port = "uci.dropbear.dropbear.@wan.Port",
  ssh_wan_pwd_auth = "uci.dropbear.dropbear.@wan.PasswordAuth",
  ssh_wan_root_pwd_auth = "uci.dropbear.dropbear.@wan.RootPasswordAuth",
  ssh_wan_src_ip = "uci.dropbear.dropbear.@wan.AllowedClientIPs",
}
local ssh_valid = {
  ssh_lan_enable = vB,
  ssh_lan_pwd_auth = gVIES(authentication_choices),
  ssh_lan_root_pwd_auth = gVIES(authentication_choices),
  ssh_wan_enable = vB,
  ssh_wan_port = vSIP,
  ssh_wan_pwd_auth = gVIES(authentication_choices),
  ssh_wan_root_pwd_auth = gVIES(authentication_choices),
  ssh_wan_src_ip = gOV(vSII)
}
local ssh_params, ssh_helpmsg = post_helper.handleQuery(ssh_params, ssh_valid)

-- go through rules to find SSH-wan anonymous section name
local ssh_wan_fw_rule_index = ""
local firewall_rules = proxy.get("uci.firewall.rule.")
local firewall_rules_path = content_helper.convertResultToObject("uci.firewall.rule.",firewall_rules)
local firewall_rules_count = 0
local create_ssh_wan_fw_rule = true
for _, v in ipairs(firewall_rules_path) do
  firewall_rules_count = firewall_rules_count + 1
  if (v["name"] == "SSH-wan" or v["name"] == "SSH_wan") then
    ssh_wan_fw_rule_index = v["paramindex"]
    create_ssh_wan_fw_rule = false
    ngx.log(ngx.ALERT, "SSH-wan firewall rule found @ "..ssh_wan_fw_rule_index.." src_ip = "..ssh_wan_src_ip)
    break
  end
end
if ssh_wan_fw_rule_index == "" then
  ssh_wan_fw_rule_index="@rule["..firewall_rules_count.."]"
  ngx.log(ngx.ALERT, "SSH-wan firewall rule NOT FOUND index set to "..ssh_wan_fw_rule_index)
end


if ngx.var.request_method == "POST" then
  local vars = ngx.req.get_post_args()
  if vars["action"] == "SAVE" then
    local ssh_wan_enable_new = untaint(vars["ssh_wan_enable"])
    local ssh_wan_port_new = untaint(vars["ssh_wan_port"])
    local ssh_wan_src_ip_new = untaint(vars["ssh_wan_src_ip"])
    ngx.log(ngx.ALERT, "ssh_wan_enable     = "..ssh_wan_enable.." ssh_wan_port     = "..ssh_wan_port.." ssh_wan_src_ip     = "..ssh_wan_src_ip)
    ngx.log(ngx.ALERT, "ssh_wan_enable_new = "..ssh_wan_enable_new.." ssh_wan_port_new = "..ssh_wan_port_new.." ssh_wan_src_ip_new = "..ssh_wan_src_ip_new)
    if ssh_wan_enable_new ~= ssh_wan_enable or ssh_wan_port_new ~= ssh_wan_port or ssh_wan_src_ip ~= ssh_wan_src_ip_new then
      local target
      if ssh_wan_enable_new == "1" then
        target = "ACCEPT"
      else
        target = "DROP"
        ssh_wan_src_ip_new = ""
      end
      if create_ssh_wan_fw_rule then
        local key = proxy.add("uci.firewall.rule.")
        proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".name", "SSH-wan")
        proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".src", "wan")
      end
      proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".src_ip", ssh_wan_src_ip_new)
      proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".dest_port", ssh_wan_port_new)
      proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".target", target)
      proxy.apply()
    end
  end
end

local function resetreboot(path,value)
  proxy.set(path, value)
  ngx.header.content_type = "application/json"
  ngx.print('{ "success":"true" }')
  ngx.timer.at(0, function()
    proxy.apply()
  end)
  ngx.exit(ngx.HTTP_OK)
end

-- Handle fwupgrade/configimport/configexport specific GET/POSTs.
if ngx.req.get_method() == "POST" then
  local action = ngx.req.get_post_args().action
  if action == "system_bootp" then
    return resetreboot("sys.bootp.enable","1")
  end
  if action == "system_switchover" then
    return resetreboot("rpc.system.switchover","1")
  end
end

local switch_class_enable = {
  input = {
    class = "monitor-changes",
  }
}

  ngx.print('\
');  ngx.print(ui_helper.createHeader(T"System Extras", false, false))   ngx.print('\
<div class="modal-body update">\
');
ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  ngx.print('\
<form class="form-horizontal" method="post" action="modals/system-modal.lp">\
<fieldset>\
  <legend>');  ngx.print( T"Advanced Reboot" ); ngx.print('</legend>\
    ');
    local bootp_alert = {
      alert = {
        class = "alert-info hide",
        id = "bootp-msg"
      }
    }
    local bootp = {
      group = {
        class = "wait-for-reboot"
      },
      button = {
        id = "btn-system-bootp",
        class = "custom-handler"
      }
    }
	  local switchover_alert = {
      alert = {
        class = "alert-info hide",
        id = "switchover-msg"
      }
    }
    local switchover = {
      group = {
        class = "wait-for-reboot"
      },
      button = {
        id = "btn-system-switchover",
        class = "custom-handler"
      }
    }
    local html = {}
    html[#html + 1] = ui_helper.createButton(T"LAN Update", T"BOOTP", "icon-exchange", bootp)
    html[#html + 1] = ui_helper.createAlertBlock(T"Entering BOOTP, please wait...", bootp_alert)
    html[#html + 1] = ui_helper.createButton(T"SwitchBank", T"Switchover", "icon-exchange", switchover)
    html[#html + 1] = ui_helper.createAlertBlock(T"Switching bank, please wait...", switchover_alert)
    ngx.print(html)
    ngx.print('\
</fieldset>\
<fieldset>\
  <legend>');  ngx.print( T"SSH Access" ); ngx.print('</legend>\
    ');
    local ssh_lan_input = {
      group = {
        class ="monitor-ssh_lan_enable monitor-1",
      },
    }
    local ssh_wan_input = {
      group = {
        class ="monitor-ssh_wan_enable monitor-1",
      },
    }
    local port_attributes = {
      group = {
        class ="monitor-ssh_wan_enable monitor-1",
      },
      input = {
        type = "number",
        min = "1",
        max = "65535",
        style = "width:70px",
      }
    }
    local src_ip_attributes = {
      group = {
        class ="monitor-ssh_wan_enable monitor-1",
      },
      input = {
        maxlength= "45",
        class = "span5",
      }
    }
    local html = {}
      html[#html + 1] = ui_helper.createSwitch(T"LAN", "ssh_lan_enable", ssh_params["ssh_lan_enable"], switch_class_enable)
      html[#html + 1] = ui_helper.createInputSelect(T"LAN User Authentication", "ssh_lan_pwd_auth", authentication_choices, ssh_params["ssh_lan_pwd_auth"], ssh_lan_input, ssh_helpmsg["ssh_lan_pwd_auth"])
      html[#html + 1] = ui_helper.createInputSelect(T"LAN Root Authentication", "ssh_lan_root_pwd_auth", authentication_choices, ssh_params["ssh_lan_root_pwd_auth"], ssh_lan_input, ssh_helpmsg["ssh_lan_root_pwd_auth"])
      html[#html + 1] = ui_helper.createSwitch(T"WAN", "ssh_wan_enable", ssh_params["ssh_wan_enable"], switch_class_enable)
      html[#html + 1] = ui_helper.createInputSelect(T"WAN User Authentication", "ssh_wan_pwd_auth", authentication_choices, ssh_params["ssh_wan_pwd_auth"], ssh_wan_input, ssh_helpmsg["ssh_wan_pwd_auth"])
      html[#html + 1] = ui_helper.createInputSelect(T"WAN Root Authentication", "ssh_wan_root_pwd_auth", authentication_choices, ssh_params["ssh_wan_root_pwd_auth"], ssh_wan_input, ssh_helpmsg["ssh_wan_root_pwd_auth"])
      html[#html + 1] = ui_helper.createInputText(T"WAN SSH Port", "ssh_wan_port", ssh_params["ssh_wan_port"], port_attributes, ssh_helpmsg["ssh_wan_port"])
      html[#html + 1] = ui_helper.createInputText(T"Allowed Client IP", "ssh_wan_src_ip", ssh_params["ssh_wan_src_ip"], src_ip_attributes, ssh_helpmsg["ssh_wan_src_ip"])
      ngx.print(html)
      ngx.print('\
</fieldset>\
<fieldset>\
  <legend>');  ngx.print( T"Theme" ); ngx.print('</legend>\
    ');
      local html = {}
      html[#html + 1] = ui_helper.createInputSelect(T"Theme", "theme_name", theme_names, theme_settings["THEME"])
      html[#html + 1] = ui_helper.createInputSelect(T"Colour", "theme_colour", theme_colours, theme_settings["COLOR"])
      html[#html + 1] = ui_helper.createInputSelect(T"Card Icons", "card_icons", card_icons_options, theme_settings["ICONS"])
      ngx.print(html)
      ngx.print('\
</fieldset>\
<fieldset>\
  <legend>');  ngx.print( T"System Log" ); ngx.print('</legend>\
    ');
      local html = {}
      html[#html + 1] = ui_helper.createSwitch(T"Enable Syslog", "syslog_present", syslog_present, switch_class_enable)
      local switch_class_filter = {
        group = {
          class ="monitor-syslog_present monitor-1",
        },
        input = {
          class = "monitor-changes",
        }
      }
      local Sysloginput = {
        group = {
          class ="monitor-syslog_present monitor-1",
        },
        input = {
          maxlength= "15",
          class = "span2",
        },
      }
      local Sysloginputip_fltr = {
        group = {
          class ="monitor-syslog_present monitor-1 monitor-syslog_filter_sw monitor-1",
        },
        input = {
          maxlength= "15",
          class = "span2",
        },
      }
      local Sysloginputip_fltr_fltr = {
        group = {
          class ="monitor-syslog_present monitor-1 monitor-syslog_filter_sw monitor-1",
        },
        input = {
          maxlength= "50",
          class = "span5",
         },
      }
      local filter_warning = {
        alert = {
          class = "alert-info monitor-syslog_present monitor-1 monitor-syslog_filter_sw monitor-1"
        },
      }
      html[#html+1] = [[<div class="monitor-syslog_filter_sw monitor-0 monitor-hide-syslog_filter_sw">]]
      html[#html+1] = ui_helper.createInputText(T"Server IP", "syslog_ip", syslog_params["syslog_ip"], Sysloginput, syslog_helpmsg["syslog_ip"])
      html[#html+1] = [[</div>]]
      html[#html+1] = ui_helper.createInputText(T"Server IP", "syslog_filter_ip", syslog_params["syslog_filter_ip"], Sysloginputip_fltr, syslog_helpmsg["syslog_filter_ip"])
      html[#html+1] = ui_helper.createSwitch(T"Enable Filter", "syslog_filter_sw", syslog_filter_sw, switch_class_filter)
      html[#html + 1] = ui_helper.createAlertBlock(T"Every filter is a regular expression string; Start with '!' to invert selection", filter_warning)
      html[#html + 1] = format('<div class="control-group"><label class="control-label monitor-syslog_present monitor-1 monitor-syslog_filter_sw monitor-1">%s</label><div class="controls">', T"Syslog Filters")
      html[#html+1] = ui_helper.createTable(filter_columns, filter_data, filter_options, Sysloginputip_fltr_fltr, filter_helpmsg)
      html[#html + 1] = '</div></div>'
      html[#html+1] = ui_helper.createInputText(T"Server Port", "syslog_port", syslog_params["syslog_port"], Sysloginput, syslog_helpmsg["syslog_port"])
      ngx.print(html)
      ngx.print('\
</fieldset></form>\
</div>\
<script>\
(function() {\
  var refreshTimeOut = 5000;\
  var refreshDelay = 3000;\
  var target = $(".modal form").attr("action");\
  function wait_for_webserver_running() {\
    $.ajax({ url: "/", timeout: refreshTimeOut, data: { check: "1"} })\
    .done(function(data) {\
      document.open("text/html");\
      document.write(data);\
      document.close();\
    })\
    .fail(function() {\
      window.setTimeout(wait_for_webserver_running, refreshDelay);\
    });\
  }\
  function wait_for_webserver_down() {\
    $.ajax({ url: target, timeout: refreshTimeOut, data: { check: "1"}  })\
    .done(function() {\
      window.setTimeout(wait_for_webserver_down, refreshDelay);\
    })\
    .fail(function() {\
      window.setTimeout(wait_for_webserver_running, refreshDelay);\
    });\
  }\
  function resetreboot(msg, msg_dst, action) {\
    msg_dst.after(msg);\
    msg.removeClass("hide");\
    msg[0].scrollIntoView();\
    $.post(\
      target,\
      { action: action, CSRFtoken: $("meta[name=CSRFtoken]").attr("content") },\
      wait_for_webserver_down,\
      "json"\
    );\
    return false;\
  }\
  $("#btn-system-bootp").on("click", function() {\
     if (confirm("');  ngx.print(T"This will put your gateway into LAN Update"); ngx.print('")){\
       tch.showProgress(\'');  ngx.print(T"Rebooting..."); ngx.print('\');\
       return resetreboot($("#bootp-msg"), $(this), "system_bootp");\
     }\
  });\
  $("#btn-system-switchover").on("click", function() {\
     if (confirm("');  ngx.print(T"This will switch the active bank with the passive one"); ngx.print('")){\
       tch.showProgress(\'');  ngx.print(T"Rebooting..."); ngx.print('\');\
       return resetreboot($("#switchover-msg"), $(this), "system_switchover");\
     }\
  });\
  $("[name=syslog_present]").on("change", function(){\
    $("[name=syslog_ip]").val("");\
    $("[name=syslog_filter_ip]").val("");\
  });\
  $("[name=syslog_filter_sw]").on("change", function(){\
     if ($( this ).val() == "0"){\
       $("[name=syslog_ip]").val($("[name=syslog_filter_ip]").val());\
       $("[name=syslog_filter_ip]").val("");\
       $("[name=syslog_filter]").val("daemon");\
     }\
     else{\
       $("[name=syslog_filter_ip]").val($("[name=syslog_ip]").val());\
       $("[name=syslog_ip]").val("");\
       $("[name=syslog_filter]").val("daemon");\
     }\
  });\
}());\
</script>\
');  ngx.print(ui_helper.createFooter());   ngx.print('\
');
