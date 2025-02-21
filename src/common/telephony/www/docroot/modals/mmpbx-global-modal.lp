--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-voice')

local ngx = ngx
local proxy = require ("datamodel")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local content_helper = require("web.content_helper")
local uimessage_helper = require("web.uimessage_helper")
local format = string.format
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local uniqueKey = post_helper.getRandomKey()
local vB = post_helper.validateBoolean

local cfg = {
  wansensing_enabled = "uci.wansensing.global.enable",
  mobile_devices = "rpc.mobiled.DeviceNumberOfEntries",
}
content_helper.getExactContent(cfg)
local mobile_devices = tonumber(cfg["mobile_devices"]) or 0

local mapValid = {
  mmpbx_enabled = vB,
  voiceonfailover = vB,
}
local mapParams = {
  mmpbx_enabled = "uci.mmpbx.mmpbx.@global.enabled",
  voiceonfailover = "uci.wansensing.global.voiceonfailover",
}
if mobile_devices == 0 then
  mapParams.voiceonfailover = nil
  mapValid.voiceonfailover = nil
end
local content = post_helper.handleQuery(mapParams,mapValid)

local split = require("split").split

local network_rpc_path = "rpc.network.interface."
local network_rpc_content = content_helper.getMatchedContent(network_rpc_path)

local wan_intfs = {}
for _,v in ipairs (network_rpc_content) do
  local path = split(split(format("%s",v.path),"@")[2],"%.")
  if v.type == "wan" or v.type == "lan" then
    if v.proto ~= "dhcpv6" and v.proto ~= "6rd" then
      wan_intfs[#wan_intfs+1] = { path[1],T(path[1]) }
    end
  end
end

-- SIP Networks
local sipnet_columns = {
  { -- [1]
    header = T"Name",
    name = "user_friendly_name",
    param = "user_friendly_name",
    type = "text",
    unique = true,
    attr = { input = { class="span2 no-save" }},
  },
  { -- [2]
    header = T"Primary<br>Registrar",
    name = "primaryRegistrar",
    param = "primary_registrar",
    type = "text",
    attr = { input = { class="span2 no-save" } },
  },
  { -- [3]
    header = T"Domain Name",
    name = "domainName",
    param = "domain_name",
    type = "text",
    attr = { input = { class="span2 no-save" } },
  },
  { -- [4]
    header = T"Realm",
    name = "realm",
    param = "realm",
    type = "text",
    attr = { input = { class="span2 no-save" } },
  },
  { -- [5]
    header = T"Primary<br>RegistrarPort",
    name = "primaryRegistrarPort",
    param = "primary_registrar_port",
    type = "text",
    default = "5060",
    attr = { input = { class="span1 no-save" } },
  },
  { -- [6]
    header = T"Primary Proxy",
    name = "primaryProxy",
    param = "primary_proxy",
    type = "text",
    attr = { input = { class="span2 no-save" } },
  },
  { -- [7]
    header = T"Primary<br>ProxyPort",
    name = "primaryProxyPort",
    param = "primary_proxy_port",
    type = "text",
    default = "0",
    attr = { input = { class="span1 no-save" } },
  },
  { -- [8]
    header = T"Local<br>Port",
    name = "localPort",
    param = "local_port",
    type = "text",
    unique = true,
    attr = { input = { class="span1 no-save" } },
  },
  { -- [9]
    header = T"Expire<br>Time",
    name = "regExpire",
    param = "reg_expire",
    type = "text",
    default = "180",
    attr = { input = { class="span1 no-save" } },
  },
  { -- [10]
    header = T"Network<br>Interface",
    name = "Iface",
    param = "interface",
    default = "wan",
    type = "select",
    values = wan_intfs,
    attr = { select = { class="span2 centered no-save" } },
    readonly = false,
  },
}

local gOV = post_helper.getOptionalValidation
local gOr = post_helper.getOrValidation

local sipnet_valid = {
  primaryRegistrarPort = post_helper.validateStringIsPort,
  primaryRegistrar = gOr(post_helper.validateStringIsIP,post_helper.validateStringIsDomainName),
  domainName = gOV(gOr(post_helper.validateStringIsIP,post_helper.validateStringIsDomainName)),
  realm = gOV(gOr(post_helper.validateStringIsIP,post_helper.validateStringIsDomainName)),
  primaryProxy = gOV(gOr(post_helper.validateStringIsIP,post_helper.validateStringIsDomainName)),
  primaryProxyPort = post_helper.getValidateNumberInRange(0,65535),
  regExpire = post_helper.validateRegExpire,
  Interface = post_helper.getValidateInEnumSelect(wan_intfs),
}

local function onAddVoice(current_section_index,content)
  local variant = proxy.get("uci.env.var.prod_friendly_name")[1].value
  local version = proxy.get("uci.version.version.@version[0].version")[1].value
  local suffix = proxy.get("uci.versioncusto.override.fwversion_suffix")[1].value
  local shortVersion = string.sub(version,1,string.find(version,"-"))

  if not content.primaryProxy or ( untaint(content.primaryProxy) == "" ) then
    proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".primary_proxy","0.0.0.0")
  end
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".401_407_waiting_time","300")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".assert_tls_connection","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".cac","1")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".call_waiting_alert_info","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".call_waiting_provisional_response","180")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".call_waiting_reject_response","486")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".certificate_depth","5")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".check_ttl_for_dns_record","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".conference_factory_uri_user_part","conf")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".conference_include_referred_by_header","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".conference_release_call_after_transfer","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".control_qos_field","dscp")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".control_qos_value","ef")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".disconnect_on_bye_response","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".dnd_response","486")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".dtmf_relay","rfc2833")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".dtmf_relay_translation","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".enable_re_register_on_cancel_timeout","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".escape_hash","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".escape_star","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".fail_behaviour","stop")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".forking_mode","replace")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".from_anonymous_handling","unavailable")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".hide_userinfo_and_port_in_subscription_request","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".hook_flash_relay","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".include_sip_instance","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".ingress_media_timeout","1000")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".interface6","wan6")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".interface_selection_strategy","ipv6_ipv4_strategy_1")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".interface_update_delay","-1")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".min_period_proxy_redundancy","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".min_session_expires","90")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".no_answer_response","480")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".optimized_authentication","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".privacy_handling","apply")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".provisional_timer","180")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".re_register_on_403","1")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".re_register_on_504","2")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".re_registration_mode","standard")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".realm_check","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".realtime_qos_field","dscp")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".realtime_qos_value","ef")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".reg_back_off_timeout","300")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".reg_expire_T_before","40")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".registration_delay","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".rejection_response","486")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".reliable_provisional_response","supported")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".remote_hold_tone_enabled","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".repeat_ringing_interval","60")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".retry_registration_on_503_policy","max_backoff_retryafter")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".rport_in_via","1")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".sdp_direction_attribute_media_level_only","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".sdp_direction_call_hold","sendonly")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".sdp_direction_call_hold_answer","recvonly")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".sdp_session_version_loose","1")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".secondary_proxy_port","5060")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".secondary_registrar_port","5060")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".session_expires","180")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".session_timer","disabled")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".sip_message_max_size","8192")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".switch_next_proxy_on_failure_response","1")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".timer_B","32000")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".timer_D","50000")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".timer_F","32000")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".timer_J","32000")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".timer_T1","500")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".timer_T2","4000")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".timer_T4","5000")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".tls_encryption_method","ssl_v3")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".tls_key_type","rsaprivatekey")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".tls_port","5061")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".tls_support","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".transparent_soc_transmission","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".transport_type","undefined")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".update_support","1")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".uri_clir_format","standard")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".use_domain_in_contact","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".use_domain_in_via","0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".user_agent",untaint(variant.." Build "..shortVersion..suffix))
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".voice_limit_qos_field","dscp")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".voice_limit_qos_value","cs0")
  proxy.set("uci.mmpbxrvsipnet.network.@"..current_section_index..".waiting_time_for_registration_on_400_or_503_response","0")

  proxy.add("uci.mmpbx.network.",current_section_index)
  proxy.set("uci.mmpbx.network.@"..current_section_index..".config","mmpbxrvsipnet")

  proxy.get("uci.mmpbx.") --workaround to avoid HTTP 500 error

  local tempindex = proxy.add("uci.mmpbx.dial_plan.@dial_plan_generic.network.")
  if tempindex then
    proxy.set("uci.mmpbx.dial_plan.@dial_plan_generic.network.@"..tempindex..".value",current_section_index)
  end

  tempindex = proxy.add("uci.mmpbx.scc.@scc_generic.network.")
  if tempindex then
    proxy.set("uci.mmpbx.scc.@scc_generic.network.@"..tempindex..".value",current_section_index)
  end

  tempindex = proxy.add("uci.mmpbx.media_filter.@media_filter_audio_generic.network.")
  if tempindex then
    proxy.set("uci.mmpbx.media_filter.@media_filter_audio_generic.network.@"..tempindex..".value",current_section_index)
  end

  for _ ,key in pairs(proxy.getPN("uci.mmpbx.audionotification.",true)) do
    tempindex = proxy.add(key.path.."network.")
    if tempindex then
      proxy.set(key.path.."network.@"..tempindex..".value",current_section_index)
    end
  end
end

local function onDeleteVoice(current_section_index)
  local function getIndex(path,current_section_index)
    local pn = proxy.getPN(path,true)
    if pn then
      for _ ,key in pairs(pn) do
        if proxy.get(key.path.."value")[1].value == current_section_index then
          return key.path
        end
      end
    end
    return nil
  end

  proxy.del("uci.mmpbx.network.@"..current_section_index..".")

  proxy.del(getIndex("uci.mmpbx.dial_plan.@dial_plan_generic.network.",current_section_index))
  proxy.del(getIndex("uci.mmpbx.scc.@scc_generic.network.",current_section_index))
  proxy.del(getIndex("uci.mmpbx.media_filter.@media_filter_audio_generic.network.",current_section_index))

  for _ ,key in pairs(proxy.getPN("uci.mmpbx.audionotification.",true)) do
    proxy.del(getIndex(key.path.."network.",current_section_index))
  end
end

local sipnet_options = {
  tableid = "sipnet",
  basepath = "uci.mmpbxrvsipnet.network.@.",
  sorted = "user_friendly_name",--DO NO ACTIVATE SORTING OR WILL BREAK DELETE
  objectName  = "sip_net"..uniqueKey,
  addNamedObject = true,
  onAdd = onAddVoice,
  onDelete = onDeleteVoice,
  maxEntries = 1,
  minEntries = 1,
  canAdd = false,
  canDelete = false,
}

if mobile_devices == 0 and cfg["wansensing_enabled"] == "1" and content["voiceonfailover"] == "1" then
  sipnet_columns[10]["readonly"] = true
  sipnet_columns[10]["header"] = sipnet_columns[10]["header"].." <span class='icon-question-sign' title='Interface is controlled by WAN Sensing.'></span>"
end

local sipnet_data,sipnet_helpmsg = post_helper.handleTableQuery(sipnet_columns,sipnet_options,nil,nil,sipnet_valid)

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
    uri = uri.."?"..ngx.var.args
end

ngx.print(ui_helper.createHeader(T"Telephony",false,true));
ngx.print('\
<div class="modal-body update">\
  ');
local lp = require("web.lp")
lp.setpath("/www/snippets/")
lp.include("tabs-voice.lp")
ngx.print('\
  <form class="form-horizontal" method="post" action="');ngx.print(uri); ngx.print('">'); ngx.print(
    ui_helper.createMessages(uimessage_helper.popMessages()));
  local html = {}

  html[#html+1] = '<fieldset id="mmpbx-global-tab">'
  html[#html+1] =     '<legend>'..T"Global Configuration"..'</legend>'
  html[#html+1] =     ui_helper.createSwitch(T"Telephony Enabled","mmpbx_enabled",content["mmpbx_enabled"])
  if cfg["wansensing_enabled"] == "1" and mobile_devices > 0 then
    html[#html+1] =     ui_helper.createSwitch(T"Voice on 4G Failover","voiceonfailover",content["voiceonfailover"])
  elseif content["voiceonfailover"] then
    html[#html + 1] =   ui_helper.createSimpleInputHidden("voiceonfailover",content["voiceonfailover"])
  end
  html[#html+1] = '<fieldset>'
  html[#html+1] = '<legend>'..T"SIP Network"..'</legend>'
  html[#html+1] =  ui_helper.createTable(sipnet_columns,sipnet_data,sipnet_options,nil,sipnet_helpmsg)
  html[#html+1] = '</fieldset>'
  ngx.print (html)

  ngx.print('\
    </form>\
</div>\
'); ngx.print(
    ui_helper.createFooter()
);
