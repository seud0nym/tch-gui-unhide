--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ngx,string = ngx,string
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local format = string.format
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local gOV = post_helper.getOptionalValidation
local gVIES = post_helper.getValidateInEnumSelect
local gVIPIL = post_helper.getValidationIfPropInList
local vB = post_helper.validateBoolean
local vSII = post_helper.validateStringIsIP
local vSIP = post_helper.validateStringIsPort
local vSIPR = post_helper.validateStringIsPortRange

local isBridgedMode = require("bridgedmode_helper").isBridgedMode()

local syslog_present,syslog_filter_sw = "0","0"
local syslog = proxy.get("uci.system.system.@system[0].log_ip","uci.system.system.@system[0].log_filter_ip")
if syslog[1].value ~= "" or syslog[2].value ~= "" then
  syslog_present = "1"
  if syslog[2].value ~= "" then
    syslog_filter_sw = "1"
  end
end

if ngx.var.request_method == "POST" then
  local vars = ngx.req.get_post_args()
  if vars["action"] == "SAVE" then
    syslog_present = vars["syslog_present"]
    syslog_filter_sw = vars["syslog_filter_sw"]
    proxy.set("uci.system.system.@system[0].log_ip","")
    proxy.set("uci.system.system.@system[0].log_filter_ip","")
    proxy.apply()
  end
end

local function validateFilter(_,_,_)
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
  createMsg = T"Add Syslog Filter",
  minEntries = 1,
  maxEntries = 10,
}
local filter_data,filter_helpmsg = post_helper.handleTableQuery(filter_columns,filter_options,nil,nil,filter_valid)

local syslog_params = {
  syslog_filter_ip = "uci.system.system.@system[0].log_filter_ip",
  syslog_port = "uci.system.system.@system[0].log_port",
  syslog_ip = "uci.system.system.@system[0].log_ip",
}
local syslog_valid = {
    syslog_present = vB,
    syslog_filter_sw = vB,
    syslog_ip = gVIPIL(gVIPIL(gOV(vSII),"syslog_filter_sw",{"0"}),"syslog_present",{"1"}),
    syslog_filter_ip = gVIPIL(gVIPIL(gOV(vSII),"syslog_filter_sw",{"1"}),"syslog_present",{"1"}),
    syslog_port = gVIPIL(vSIPR,"syslog_present",{"1"}),
    post_helper.validateStringIsPortRange
}
local syslog_data,syslog_helpmsg = post_helper.handleQuery(syslog_params,syslog_valid)

local proto_choices = {
  {"TCP",T"TCP"},
  {"UDP",T"UDP"},
}

local iperf_available = proxy.get("uci.iperf.iperf.enabled")
local iperf_params
local iperf_valid
if iperf_available then
  iperf_params = {
    iperf_enable = "uci.iperf.iperf.enabled",
    iperf_port = "uci.iperf.iperf.dest_port",
    iperf_proto = "uci.iperf.iperf.proto",
  }
  iperf_valid = {
    iperf_enable = vB,
    iperf_port = vSIP,
    iperf_proto = gVIES(proto_choices),
  }
  iperf_params = post_helper.handleQuery(iperf_params,iperf_valid)
end

local authentication_choices = {
  {"on",T"Certificate or Password"},
  {"off",T"Certificate (Recommended)"},
}

local ssh_wan_enable,ssh_wan_port,ssh_wan_src_ip
local ssh_afg_if = proxy.get("uci.dropbear.dropbear.@afg.Interface")
local ssh_params = {
  ssh_lan_enable = "uci.dropbear.dropbear.@lan.enable",
  ssh_lan_pwd_auth = "uci.dropbear.dropbear.@lan.PasswordAuth",
  ssh_lan_root_pwd_auth = "uci.dropbear.dropbear.@lan.RootPasswordAuth",
}
local ssh_valid = {
  ssh_lan_enable = vB,
  ssh_lan_pwd_auth = gVIES(authentication_choices),
  ssh_lan_root_pwd_auth = gVIES(authentication_choices),
}
if not isBridgedMode then
  ssh_wan_enable = untaint(proxy.get("uci.dropbear.dropbear.@wan.enable")[1].value)
  ssh_wan_port = untaint(proxy.get("uci.dropbear.dropbear.@wan.Port")[1].value)
  ssh_wan_src_ip = proxy.get("uci.dropbear.dropbear.@wan.AllowedClientIPs")

  ssh_params["ssh_wan_enable"] = "uci.dropbear.dropbear.@wan.enable"
  ssh_params["ssh_wan_port"] = "uci.dropbear.dropbear.@wan.Port"
  ssh_params["ssh_wan_pwd_auth"] = "uci.dropbear.dropbear.@wan.PasswordAuth"
  ssh_params["ssh_wan_root_pwd_auth"] = "uci.dropbear.dropbear.@wan.RootPasswordAuth"
  ssh_valid["ssh_wan_enable"] = vB
  ssh_valid["ssh_wan_port"] = vSIP
  ssh_valid["ssh_wan_pwd_auth"] = gVIES(authentication_choices)
  ssh_valid["ssh_wan_root_pwd_auth"] = gVIES(authentication_choices)

  if ssh_wan_src_ip then
    ssh_params["ssh_wan_src_ip"] = "uci.dropbear.dropbear.@wan.AllowedClientIPs"
    ssh_valid["ssh_wan_src_ip"] = gOV(vSII)
  end
end
if ssh_afg_if then
  ssh_params["ssh_afg_enable"] = "uci.dropbear.dropbear.@afg.enable"
  ssh_params["ssh_afg_pwd_auth"] = "uci.dropbear.dropbear.@afg.PasswordAuth"
  ssh_params["ssh_afg_root_pwd_auth"] = "uci.dropbear.dropbear.@afg.RootPasswordAuth"
  ssh_valid["ssh_afg_enable"] = vB
  ssh_valid["ssh_afg_pwd_auth"] = gVIES(authentication_choices)
  ssh_valid["ssh_afg_root_pwd_auth"] = gVIES(authentication_choices)
end
local ssh_data,ssh_helpmsg = post_helper.handleQuery(ssh_params,ssh_valid)

if ssh_data.ssh_lan_pwd_auth then
  if ssh_data.ssh_lan_pwd_auth == "0" or ssh_data.ssh_lan_pwd_auth == "" then
    ssh_data.ssh_lan_pwd_auth = "off"
  elseif ssh_data.ssh_lan_pwd_auth == "1" then
    ssh_data.ssh_lan_pwd_auth = "on"
  end
end
if ssh_data.ssh_lan_root_pwd_auth then
  if ssh_data.ssh_lan_root_pwd_auth == "0" or ssh_data.ssh_lan_root_pwd_auth == "" then
    ssh_data.ssh_lan_root_pwd_auth = "off"
  elseif ssh_data.ssh_lan_root_pwd_auth == "1" then
    ssh_data.ssh_lan_root_pwd_auth = "on"
  end
end
if ssh_data.ssh_wan_pwd_auth then
  if ssh_data.ssh_wan_pwd_auth == "0" or ssh_data.ssh_wan_pwd_auth == "" then
    ssh_data.ssh_wan_pwd_auth = "off"
  elseif ssh_data.ssh_wan_pwd_auth == "1" then
    ssh_data.ssh_wan_pwd_auth = "on"
  end
end
if ssh_data.ssh_wan_root_pwd_auth then
  if ssh_data.ssh_wan_root_pwd_auth == "0" or ssh_data.ssh_wan_root_pwd_auth == "" then
    ssh_data.ssh_wan_root_pwd_auth = "off"
  elseif ssh_data.ssh_wan_root_pwd_auth == "1" then
    ssh_data.ssh_wan_root_pwd_auth = "on"
  end
end
if ssh_data.ssh_afg_pwd_auth then
  if ssh_data.ssh_afg_pwd_auth == "0" or ssh_data.ssh_afg_pwd_auth == "" then
    ssh_data.ssh_afg_pwd_auth = "off"
  elseif ssh_data.ssh_afg_pwd_auth == "1" then
    ssh_data.ssh_afg_pwd_auth = "on"
  end
end
if ssh_data.ssh_afg_root_pwd_auth then
  if ssh_data.ssh_afg_root_pwd_auth == "0" or ssh_data.ssh_afg_root_pwd_auth == "" then
    ssh_data.ssh_afg_root_pwd_auth = "off"
  elseif ssh_data.ssh_afg_root_pwd_auth == "1" then
    ssh_data.ssh_afg_root_pwd_auth = "on"
  end
end

if not isBridgedMode and ngx.var.request_method == "POST" then
  local vars = ngx.req.get_post_args()
  if vars["action"] == "SAVE" then
    local ssh_wan_enable_new = untaint(vars["ssh_wan_enable"])
    local ssh_wan_port_new = untaint(vars["ssh_wan_port"])
    local ssh_wan_src_ip_new = untaint(vars["ssh_wan_src_ip"])
    if ssh_wan_enable_new ~= ssh_wan_enable or ssh_wan_port_new ~= ssh_wan_port or (not ssh_wan_src_ip and ssh_wan_src_ip_new ~= "") or untaint(ssh_wan_src_ip[1].value) ~= ssh_wan_src_ip_new then
      -- go through rules to find SSH-wan anonymous section name
      local ssh_wan_fw_rule_index = ""
      local firewall_rules = proxy.get("uci.firewall.rule.")
      local firewall_rules_path = content_helper.convertResultToObject("uci.firewall.rule.",firewall_rules)
      local firewall_rules_count = 0
      local create_ssh_wan_fw_rule = true
      for _,v in ipairs(firewall_rules_path) do
        firewall_rules_count = firewall_rules_count + 1
        if (v["name"] == "SSH-wan" or v["name"] == "SSH_wan") then
          ssh_wan_fw_rule_index = v["paramindex"]
          create_ssh_wan_fw_rule = false
          break
        end
      end
      if ssh_wan_fw_rule_index == "" then
        ssh_wan_fw_rule_index="@rule["..firewall_rules_count.."]"
      end
      local target
      if ssh_wan_enable_new == "1" then
        target = "ACCEPT"
      else
        target = "DROP"
        ssh_wan_src_ip_new = ""
      end
      if create_ssh_wan_fw_rule then
        local key = proxy.add("uci.firewall.rule.")
        if tonumber(key) then -- FW 17.2 support
          ssh_wan_fw_rule_index = format("%s",key)
        end
        proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".name","SSH-wan")
        proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".src","wan")
      end
      if ssh_wan_src_ip then
        proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".src_ip",ssh_wan_src_ip_new)
      end
      proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".dest_port",ssh_wan_port_new)
      proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".target",target)
      proxy.apply()
    end
  end
end

local function resetreboot(path,value)
  proxy.set(path,value)
  ngx.header.content_type = "application/json"
  ngx.print('{ "success":"true" }')
  ngx.timer.at(0,function(_)
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
local lose_root_warning = {
  alert = {
    class = "alert-danger",
    style = "margin-bottom:5px;"
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
local ssh_lan_input = {
  group = {
    class ="monitor-ssh_lan_enable monitor-1",
  },
}
local switch_class_filter = {
  group = {
    class ="monitor-syslog_present monitor-1",
  },
  input = {
    class = "monitor-changes",
  }
}
local sysloginput = {
  group = {
    class ="monitor-syslog_present monitor-1",
  },
  input = {
    maxlength= "15",
    class = "span2",
  },
}
local sysloginputip_fltr = {
  group = {
    class ="monitor-syslog_present monitor-1 monitor-syslog_filter_sw monitor-1",
  },
  input = {
    maxlength= "15",
    class = "span2",
  },
}
local sysloginputip_fltr_fltr = {
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

ngx.print(ui_helper.createHeader(T"System Extras",false,false),'\
<div class="modal-body update">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
  <form class="form-horizontal" method="post" action="modals/system-modal.lp">\
    <fieldset>\
      <legend>',T"Advanced Reboot",'</legend>',
      ui_helper.createButton(T"LAN Update",T"BOOTP","icon-exchange",bootp),
      ui_helper.createAlertBlock(T"Entering BOOTP. Please wait...",bootp_alert),
      ui_helper.createAlertBlock(T"Root access will be <i>lost</i> when using the <b><i class='icon-exchange' style='width:auto;'></i> Switchover</b> button <i>if</i> you have not previously acquired root on the passive bank.", lose_root_warning),
      ui_helper.createButton(T"Switch Active Bank",T"Switchover","icon-exchange",switchover),
      ui_helper.createAlertBlock(T"Switching bank. Please wait...",switchover_alert), -- need to keep the quote on the next line because this line is REMOVED on FW 20.4 (165)
      '\
    </fieldset>\
    <fieldset>\
      <legend>',T"SSH Access",'</legend>\
        <fieldset><legend><small>',T"LAN Access",'</small></legend>',
          ui_helper.createSwitch(T"SSH LAN Enabled","ssh_lan_enable",ssh_data["ssh_lan_enable"],switch_class_enable),
          ui_helper.createInputSelect(T"LAN User Authentication","ssh_lan_pwd_auth",authentication_choices,ssh_data["ssh_lan_pwd_auth"],ssh_lan_input,ssh_helpmsg["ssh_lan_pwd_auth"]),
          ui_helper.createInputSelect(T"LAN Root Authentication","ssh_lan_root_pwd_auth",authentication_choices,ssh_data["ssh_lan_root_pwd_auth"],ssh_lan_input,ssh_helpmsg["ssh_lan_root_pwd_auth"]))
if ssh_afg_if then
  local ssh_afg_input = {
    group = {
      class ="monitor-ssh_afg_enable monitor-1",
    },
  }
  local info_box = {
    alert = {
      class = "alert alert-info",
    }
  }
  ngx.print(
          ui_helper.createAlertBlock(T(format("The AFG configuration is added by the root acquisition process to allow SSH access over the %s interface.",string.upper(ssh_afg_if[1].value))),info_box),
          ui_helper.createSwitch(T"SSH AFG Enabled","ssh_afg_enable",ssh_data["ssh_afg_enable"],switch_class_enable),
          ui_helper.createInputSelect(T"AFG User Authentication","ssh_afg_pwd_auth",authentication_choices,ssh_data["ssh_afg_pwd_auth"],ssh_afg_input,ssh_helpmsg["ssh_afg_pwd_auth"]),
          ui_helper.createInputSelect(T"AFG Root Authentication","ssh_afg_root_pwd_auth",authentication_choices,ssh_data["ssh_afg_root_pwd_auth"],ssh_afg_input,ssh_helpmsg["ssh_afg_root_pwd_auth"]))
end
ngx.print('\
        </fieldset>')
if not isBridgedMode then
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
  ngx.print('\
      <fieldset><legend><small>',T"WAN Access",'</small></legend>',
        ui_helper.createSwitch(T"SSH WAN Enabled","ssh_wan_enable",ssh_data["ssh_wan_enable"],switch_class_enable),
        ui_helper.createInputSelect(T"WAN User Authentication","ssh_wan_pwd_auth",authentication_choices,ssh_data["ssh_wan_pwd_auth"],ssh_wan_input,ssh_helpmsg["ssh_wan_pwd_auth"]),
        ui_helper.createInputSelect(T"WAN Root Authentication","ssh_wan_root_pwd_auth",authentication_choices,ssh_data["ssh_wan_root_pwd_auth"],ssh_wan_input,ssh_helpmsg["ssh_wan_root_pwd_auth"]),
        ui_helper.createInputText(T"WAN SSH Port","ssh_wan_port",ssh_data["ssh_wan_port"],port_attributes,ssh_helpmsg["ssh_wan_port"]))
  if ssh_wan_src_ip then
        ngx.print(ui_helper.createInputText(T"Allowed Client IP","ssh_wan_src_ip",ssh_data["ssh_wan_src_ip"],src_ip_attributes,ssh_helpmsg["ssh_wan_src_ip"]))
  end
  ngx.print('\
      </fieldset>')
end
ngx.print('\
    </fieldset>\
    <fieldset>\
      <legend>',T"System Log",'</legend>',
        ui_helper.createSwitch(T"Enable Syslog","syslog_present",syslog_present,switch_class_enable),'\
        <div class="monitor-syslog_filter_sw monitor-0 monitor-hide-syslog_filter_sw">',
          ui_helper.createInputText(T"Server IP","syslog_ip",syslog_data["syslog_ip"],sysloginput,syslog_helpmsg["syslog_ip"]),'\
        </div>',
        ui_helper.createInputText(T"Server IP","syslog_filter_ip",syslog_data["syslog_filter_ip"],sysloginputip_fltr,syslog_helpmsg["syslog_filter_ip"]),
        ui_helper.createSwitch(T"Enable Filter","syslog_filter_sw",syslog_filter_sw,switch_class_filter),
        ui_helper.createAlertBlock(T"Every filter is a regular expression string; Start with '!' to invert selection",filter_warning),
        format('<div class="control-group"><label class="control-label monitor-syslog_present monitor-1 monitor-syslog_filter_sw monitor-1">%s</label><div class="controls">',T"Syslog Filters"),
          ui_helper.createTable(filter_columns,filter_data,filter_options,sysloginputip_fltr_fltr,filter_helpmsg),
        '</div></div>',
        ui_helper.createInputText(T"Server Port","syslog_port",syslog_data["syslog_port"],sysloginput,syslog_helpmsg["syslog_port"]),'\
    </fieldset>');
if iperf_available then
  local iperf_input = {
    group = {
      class ="monitor-iperf_enable monitor-1",
    },
  }
  local port_attributes = {
    group = {
      class ="monitor-iperf_enable monitor-1",
    },
    input = {
      type = "number",
      min = "1",
      max = "65535",
      style = "width:70px",
    }
  }
  ngx.print('\
    <fieldset>\
      <legend>',T"iperf",'</legend>',
        ui_helper.createSwitch(T"iperf Server","iperf_enable",iperf_params["iperf_enable"],switch_class_enable),
        ui_helper.createInputText(T"Port","iperf_port",iperf_params["iperf_port"],port_attributes,ssh_helpmsg["iperf_port"]),
        ui_helper.createInputSelect(T"Protocol","iperf_proto",proto_choices,iperf_params["iperf_proto"],iperf_input,ssh_helpmsg["iperf_proto"]),'\
  </fieldset>');
end;
ngx.print('\
  </form>\
</div>\
<script>\
(function() {\
  var refreshTimeOut = 5000;\
  var refreshDelay = 3000;\
  var target = $(".modal form").attr("action");\
  function wait_for_webserver_running() {\
    $.ajax({ url: "/",timeout: refreshTimeOut,data: { check: "1"} })\
    .done(function(data) {\
      document.open("text/html");\
      document.write(data);\
      document.close();\
    })\
    .fail(function() {\
      window.setTimeout(wait_for_webserver_running,refreshDelay);\
    });\
  }\
  function wait_for_webserver_down() {\
    $.ajax({ url: target,timeout: refreshTimeOut,data: { check: "1"}  })\
    .done(function() {\
      window.setTimeout(wait_for_webserver_down,refreshDelay);\
    })\
    .fail(function() {\
      window.setTimeout(wait_for_webserver_running,refreshDelay);\
    });\
  }\
  function resetreboot(msg,msg_dst,action) {\
    msg_dst.after(msg);\
    msg.removeClass("hide");\
    msg[0].scrollIntoView();\
    $.post(\
      target,\
      { action: action,CSRFtoken: $("meta[name=CSRFtoken]").attr("content") },\
      wait_for_webserver_down,\
      "json"\
    );\
    return false;\
  }\
  $("#btn-system-bootp").on("click",function() {\
     if (confirm("',T"This will put your gateway into LAN Update",'")){\
       tch.showProgress(\'',T"Rebooting...",'\');\
       return resetreboot($("#bootp-msg"),$(this),"system_bootp");\
     }\
  });\
  $("#btn-system-switchover").on("click",function() {\
     if (confirm("',T"This will switch the active bank with the passive one",'")){\
       tch.showProgress(\'',T"Rebooting...",'\');\
       return resetreboot($("#switchover-msg"),$(this),"system_switchover");\
     }\
  });\
  $("[name=syslog_present]").on("change",function(){\
    $("[name=syslog_ip]").val("");\
    $("[name=syslog_filter_ip]").val("");\
  });\
  $("[name=syslog_filter_sw]").on("change",function(){\
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
',ui_helper.createFooter())
