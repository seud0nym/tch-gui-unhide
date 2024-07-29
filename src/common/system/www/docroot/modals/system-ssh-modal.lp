--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ngx,string = ngx,string
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local gOV = post_helper.getOptionalValidation
local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean
local vSII = post_helper.validateStringIsIP
local vSIP = post_helper.validateStringIsPort

local isBridgedMode = require("bridgedmode_helper").isBridgedMode()

local authentication_choices = {
  {"on",T"Certificate or Password"},
  {"off",T"Certificate (Recommended)"},
}
local net_iface = {
  {"wan",T"Fixed WAN"},
}
if proxy.get("uci.network.interface.@wwan.enabled") then
  net_iface[#net_iface+1] = {"wwan",T"Mobile WWAN"}
end

local ssh_params = {
  ssh_lan_enable = "uci.dropbear.dropbear.@lan.enable",
  ssh_lan_pwd_auth = "uci.dropbear.dropbear.@lan.PasswordAuth",
  ssh_lan_root_pwd_auth = "uci.dropbear.dropbear.@lan.RootPasswordAuth",
  server_public_key = "rpc.gui.ssh.server_public_key",
  server_fingerprint = "rpc.gui.ssh.server_fingerprint",
}
local ssh_valid = {
  ssh_lan_enable = vB,
  ssh_lan_pwd_auth = gVIES(authentication_choices),
  ssh_lan_root_pwd_auth = gVIES(authentication_choices),
}

local function add_ssh(iface)
  ssh_params["ssh_"..iface.."_enable"] = "uci.dropbear.dropbear.@"..iface..".enable"
  ssh_params["ssh_"..iface.."_pwd_auth"] = "uci.dropbear.dropbear.@"..iface..".PasswordAuth"
  ssh_params["ssh_"..iface.."_root_pwd_auth"] = "uci.dropbear.dropbear.@"..iface..".RootPasswordAuth"
  ssh_params["ssh_"..iface.."_src_ip"] = "uci.dropbear.dropbear.@"..iface..".AllowedClientIPs"
  ssh_valid["ssh_"..iface.."_enable"] = vB
  ssh_valid["ssh_"..iface.."_pwd_auth"] = gVIES(authentication_choices)
  ssh_valid["ssh_"..iface.."_root_pwd_auth"] = gVIES(authentication_choices)
  ssh_valid["ssh_"..iface.."_src_ip"] = gOV(vSII)
end

local ssh_afg_if = proxy.get("uci.dropbear.dropbear.@afg.Interface")
if ssh_afg_if then
  add_ssh("afg")
end

local ssh_external = {}
if not isBridgedMode then
  ssh_external["wan"] = proxy.get("uci.dropbear.dropbear.@wan.enable","uci.dropbear.dropbear.@wan.Port","uci.dropbear.dropbear.@wan.AllowedClientIPs")
  add_ssh("wan")
  ssh_params["ssh_wan_port"] = "uci.dropbear.dropbear.@wan.Port"
  ssh_valid["ssh_wan_port"] = vSIP
  ssh_params["ssh_wan_iface"] = "uci.dropbear.dropbear.@wan.Interface"
  ssh_valid["ssh_wan_iface"] = gVIES(net_iface)
end

local ssh_data,ssh_helpmsg = post_helper.handleQuery(ssh_params,ssh_valid)
for k,v in pairs(ssh_data) do
  if match(k,"^ssh_.+_pwd_auth$") then
    if v == "0" or v == "" then
      ssh_data[k] = "off"
    elseif v == "1" then
      ssh_data[k] = "on"
    end
  end
end
if not ssh_helpmsg then
  ssh_helpmsg = {}
end

local function apply_firewall_rules(ifname,vars)
  local enable_new = untaint(vars["ssh_"..ifname.."_enable"])
  local port_new = untaint(vars["ssh_"..ifname.."_port"])
  local src_ip_new = untaint(vars["ssh_"..ifname.."_src_ip"])
  if enable_new ~= untaint(ssh_external[ifname][1].value) or port_new ~= untaint(ssh_external[ifname][2].value) or src_ip_new ~= untaint(ssh_external[ifname][3].value) then
    -- go through rules to find SSH-<ifname> anonymous section name
    local ssh_wan_fw_rule_index = ""
    local firewall_rules = proxy.get("uci.firewall.rule.")
    local firewall_rules_path = content_helper.convertResultToObject("uci.firewall.rule.",firewall_rules)
    local firewall_rules_count = 0
    local create_ssh_wan_fw_rule = true
    for _,v in ipairs(firewall_rules_path) do
      firewall_rules_count = firewall_rules_count + 1
      if (v["name"] == "SSH-"..ifname or v["name"] == "SSH_"..ifname) then
        ssh_wan_fw_rule_index = v["paramindex"]
        create_ssh_wan_fw_rule = false
        break
      end
    end
    if ssh_wan_fw_rule_index == "" then
      ssh_wan_fw_rule_index="@rule["..firewall_rules_count.."]"
    end
    local target
    if enable_new == "1" then
      target = "ACCEPT"
    else
      target = "DROP"
      src_ip_new = ""
    end
    if create_ssh_wan_fw_rule then
      local key = proxy.add("uci.firewall.rule.")
      if tonumber(key) then -- FW 17.2 support
        ssh_wan_fw_rule_index = format("%s",key)
      end
      proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".name","SSH-"..ifname)
      proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".src","wan") -- wan zone includes wwan
    end
    proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".src_ip",src_ip_new)
    proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".dest_port",port_new)
    proxy.set("uci.firewall.rule."..ssh_wan_fw_rule_index..".target",target)
    proxy.apply()
  end
end

if not isBridgedMode and ngx.var.request_method == "POST" then
  local vars = ngx.req.get_post_args()
  if vars["action"] == "SAVE" then
    apply_firewall_rules("wan",vars)
  end
end

local function validate_authorized_key(value)
  if match(untaint(value),"^(ssh%-rsa)%s+([%w%+%/]+)%s*(.*)$") then
    return true
  end
  return nil,"Invalid authorized_keys format or keytype field not 'ssh-rsa'"
end
local authorized_keys_columns = {
  {
    header = T"Authorized Key",
    name = "authorized_key",
    param = "authorized_key",
    type = "text",
    attr = { input = { class="span7" } },
  },
}
local authorized_keys_valid = {
  authorized_key = validate_authorized_key,
}
local authorized_keys_options = {
  canEdit = false,
  canAdd = true,
  canDelete = true,
  tableid = "authorized_keys",
  basepath = "rpc.gui.ssh.authorized_keys.@.",
  createMsg = T"Add Authorized Key",
  onAdd = function(index,values)
    local success,errors = proxy.set("rpc.gui.ssh.authorized_keys.@"..index..".authorized_key",values.authorized_key)
    if not success then
      for _,err in ipairs(errors) do
        message_helper.pushMessage(T(string.format("Failed to set %s to '%s': %s (%s)",err.path,values.authorized_key,err.errmsg,err.errcode)),"error")
      end
    end
  end,
}
local authorized_keys_data,authorized_keys_helpmsg = post_helper.handleTableQuery(authorized_keys_columns,authorized_keys_options,nil,nil,authorized_keys_valid)


local switch_class_enable = {
  input = {
    class = "monitor-changes",
  }
}
local ssh_lan_input = {
  group = {
    class ="monitor-ssh_lan_enable monitor-1",
  },
}
local ssh_keys = {
  span = {
    class = "span7",
    style = "word-break: break-all;"
  },
}

ngx.print(ui_helper.createHeader(T"System Extras",false,false),'\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-system-extras.lp")
ngx.print(
  ui_helper.createMessages(message_helper.popMessages()),'\
  <form class="form-horizontal" method="post" action="modals/system-ssh-modal.lp">\
    <fieldset>\
      <legend>',T"SSH Server Key",'</legend>\
        <fieldset>',
          ui_helper.createLabel(T"Fingerprint",ssh_data["server_fingerprint"],ssh_keys),
          ui_helper.createLabel(T"Public Key",ssh_data["server_public_key"],ssh_keys),'\
        </fieldset>\
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
  local ssh_wan_port_attributes = {
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
  local ssh_wan_src_ip_attributes = {
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
        ui_helper.createInputSelect(T"WAN Network Interface","ssh_wan_iface",net_iface,ssh_data["ssh_wan_iface"],ssh_wan_input,ssh_helpmsg["ssh_wan_iface"]),
        ui_helper.createInputText(T"WAN SSH Port","ssh_wan_port",ssh_data["ssh_wan_port"],ssh_wan_port_attributes,ssh_helpmsg["ssh_wan_port"]),
        ui_helper.createInputText(T"Allowed Client IP","ssh_wan_src_ip",ssh_data["ssh_wan_src_ip"],ssh_wan_src_ip_attributes,ssh_helpmsg["ssh_wan_src_ip"]),'\
      </fieldset>')
end
ngx.print('\
    </fieldset>\
    <style>#authorized_keys>thead{display:none;} #authorized_keys>tbody>tr>td:nth-child(1){word-break:break-all;}</style>\
    <fieldset><legend>',T"SSH Authorized Keys",'</legend>',
      ui_helper.createTable(authorized_keys_columns,authorized_keys_data,authorized_keys_options,nil,authorized_keys_helpmsg),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter())
