--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-code')

local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")
local proxy_helper = require("proxy_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local format = string.format

local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString
local gVIES = post_helper.getValidateInEnumSelect

local function validateAuto(value, _, _)
  if value == "add" or value == "ignore" then
    return true
  end
  return nil, T"'add' or 'ignore' expected."
end

local leftid, leftid_was = proxy.get("rpc.gui.ipsec.conn.@tgu-responder-base.leftid")
if leftid then
  leftid_was = untaint(leftid[1].value)
  leftid = leftid_was
end
local ddns = proxy.get("uci.ddns.service.@myddns_ipv4.domain")
if ddns then
  ddns = untaint(ddns[1].value)
  if ddns ~= "" and ddns ~= "yourhost.example.com" and leftid ~= ddns then
    leftid = ddns
  end
end
if leftid and leftid ~= "" and leftid ~= leftid_was then
  proxy_helper.set("rpc.gui.ipsec.conn.@tgu-responder-base.leftid", leftid)
end

local mapParams = {
  debugging = "rpc.gui.ipsec.debug",
  firewall = "uci.firewall.include.@ipsec_responder.enabled",
  leftcert = "rpc.gui.ipsec.conn.@tgu-responder-base.leftcert",
  leftsendcert = "rpc.gui.ipsec.conn.@tgu-responder-base.leftsendcert",
  v1_psk_xauth_auto = "rpc.gui.ipsec.conn.@tgu-responder-ikev1-psk-xauth.auto",
  v2_mschapv2_auto = "rpc.gui.ipsec.conn.@tgu-responder-ikev2-mschapv2.auto",
  v2_psk_auto = "rpc.gui.ipsec.conn.@tgu-responder-ikev2-psk.auto",
  v1_psk_xauth_ike = "rpc.gui.ipsec.conn.@tgu-responder-ikev1-psk-xauth.ike",
  v2_mschapv2_ike = "rpc.gui.ipsec.conn.@tgu-responder-ikev2-mschapv2.ike",
  v2_psk_ike = "rpc.gui.ipsec.conn.@tgu-responder-ikev2-psk.ike",
  v1_psk_xauth_esp = "rpc.gui.ipsec.conn.@tgu-responder-ikev1-psk-xauth.esp",
  v2_mschapv2_esp = "rpc.gui.ipsec.conn.@tgu-responder-ikev2-mschapv2.esp",
  v2_psk_esp = "rpc.gui.ipsec.conn.@tgu-responder-ikev2-psk.esp",
}

local mapValid = {
  debugging = vB,
  firewall = vB,
  v1_psk_xauth_auto = validateAuto,
  v2_mschapv2_auto = validateAuto,
  v2_psk_auto = validateAuto,
}

local config,helpmsg = post_helper.handleQuery(mapParams,mapValid)

if config.firewall ~= "0" then
  config.firewall = "1"
end
if config.v1_psk_xauth_auto ~= "add" then
  config.v1_psk_xauth_auto = "ignore"
end
if config.v2_mschapv2_auto ~= "add" then
  config.v2_mschapv2_auto = "ignore"
end
if config.v2_psk_auto ~= "add" then
  config.v2_psk_auto = "ignore"
end

-- Apply any uncomitted changes
local changed = proxy.get("rpc.gui.ipsec.changed")
if changed and changed[1].value == "1" then
  proxy_helper.set("rpc.gui.ipsec.apply_changes", "1")
end

local secret_type = {
  { "PSK", T"Pre-Shared Key"},
  { "EAP", T"EAP (IKEv2)"},
  { "RSA", T"RSA Private Key"},
  { "XAUTH", T"XAUTH (IPsec/IKEv1)"},
}

local secret_columns = {
  {
    header = T"Selector (e.g. Username)",
    name = "selector",
    param = "selector",
    type = "text",
    attr = { input = { class="span4", placeholder = "Leave blank for Certificates and Pre-Shared Key" } }
  },
  {
    header = T"Type",
    name = "type",
    param = "type",
    type = "select",
    values = secret_type,
    default = "EAP",
  },
  {
    header = T"Secret (Password/Certificate)",
    name = "secret",
    param = "secret",
    type = "text",
    attr = { input = { class="span4" } }
  },
}

local secret_valid = {
  enabled = vB,
  type = gVIES(secret_type),
  secret = vNES,
}

local secret_apply = function(_, _)
  proxy_helper.set("rpc.gui.ipsec.apply_changes", "1")
end

local secret_options = {
  tableid = "secrets",
  basepath = "rpc.gui.ipsec.secret.@.",
  canAdd = true,
  canDelete = true,
  canEdit = true,
  createMsg = T"Add secret",
  onAdd = secret_apply,
  onDelete = secret_apply,
  onEdit = secret_apply,
}

local psk
local secret_filter = function(data)
  if data.type == "RSA" and data.selector == "" then
    return { canDelete = false, canEdit = false }
  elseif data.type == "PSK" and data.selector == "" then
    psk = data.secret
    return { canDelete = false, canEdit = false }
  end
  return true
end

local secret_data,secret_helpmsg = post_helper.handleTableQuery(secret_columns,secret_options,secret_filter,nil,secret_valid)

if ngx.var.request_method == "POST" then
  local vars = ngx.req.get_post_args()
  if vars["action"] == "SAVE" then
    if type(vars["regenpsk"]) == "table" then
      for _,checked in pairs(vars["regenpsk"]) do
        if checked == "_TRUE_" then
          psk = nil
          break
        end
      end
    end
  end
end

if not psk then
  proxy_helper.set("rpc.gui.ipsec.regenpsk", "1")
  proxy_helper.set("rpc.gui.ipsec.apply_changes", "1")
  secret_data,secret_helpmsg = post_helper.handleTableQuery(secret_columns,secret_options,secret_filter,nil,secret_valid)
end

local responder_auto_attr = {
  values = {
    on = "add",
    off = "ignore"
  }
}
local info_attr = {
  alert = {
    class = "alert-info span8",
    style = "float:right;"
  }
}

local function alert_block(text, keybase)
  return ui_helper.createAlertBlock(T(format("%s<br><small> - IKE Cipher Suites: %s<br> - ESP Cipher Suites: %s</small>",text,config[keybase.."_ike"],config[keybase.."_esp"])),info_attr)
end

ngx.print('\
');  ngx.print(ui_helper.createHeader("IPsec VPN Server",false,true) ); ngx.print('\
<div class="modal-body update">\
  <form class="form-horizontal" method="post" action="modals/ipsec-modal.lp">\
    ');
    local html = {}
    html[#html+1] = ui_helper.createMessages(message_helper.popMessages())
    html[#html+1] = "<fieldset><legend>" .. T"Configuration" .. "</legend>"
    html[#html+1] = ui_helper.createSwitch(T"Incoming Firewall Ports","firewall",config["firewall"],nil,helpmsg["firewall"])
    html[#html+1] = ui_helper.createLabel(T"Domain<span class='icon-question-sign' title='Configure the Domain name in WAN Services under IPv4 Dynamic DNS'></span>",leftid)
    html[#html+1] = ui_helper.createLabel(T"Domain Certificate<span class='icon-question-sign' title='Configure Server Certificates in WAN Services'></span>",config["leftcert"])
    html[#html+1] = ui_helper.createLabel(T"Send Certificate",config["leftsendcert"])
    html[#html+1] = ui_helper.createSwitch(T"Debugging","debugging",config["debugging"],nil,helpmsg["debugging"])
    html[#html+1] = "</fieldset>"
    html[#html+1] = "<fieldset><legend>" .. T"Connection/Authentication Methods" .. "</legend>"
    html[#html+1] = alert_block(T"For Windows and iOS clients authorised by EAP username/password","v2_mschapv2")
    html[#html+1] = ui_helper.createSwitch(T"IKEv2/EAP-MSCHAPv2","v2_mschapv2_auto",config["v2_mschapv2_auto"],responder_auto_attr,helpmsg["v2_mschapv2_auto"])
    html[#html+1] = alert_block(T"For iOS and Android clients authorised by pre-shared key","v2_psk")
    html[#html+1] = ui_helper.createSwitch(T"IKEv2/PSK","v2_psk_auto",config["v2_psk_auto"],responder_auto_attr,helpmsg["v2_psk_auto"])
    html[#html+1] = alert_block(T"For Android IPsec clients authorised by XAUTH username/password and pre-shared key","v1_psk_xauth")
    html[#html+1] = ui_helper.createSwitch(T"IPSec/Xauth/PSK","v1_psk_xauth_auto",config["v1_psk_xauth_auto"],responder_auto_attr,helpmsg["v1_psk_xauth_auto"])
    html[#html+1] = "</fieldset>"
    html[#html+1] = "<fieldset><legend>" .. T"Secrets" .. "</legend>"
    html[#html+1] = ui_helper.createCheckboxSwitch(T"New Pre-Shared Key","regenpsk","0")
    html[#html+1] = ui_helper.createTable(secret_columns,secret_data,secret_options,nil,secret_helpmsg)
    html[#html+1] = "</fieldset>"
    ngx.print(html)
    ngx.print('\
  </form>\
</div>\
'); ngx.print(ui_helper.createFooter()); ngx.print('\
');
