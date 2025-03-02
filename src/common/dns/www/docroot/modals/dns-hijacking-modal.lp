--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local content_helper = require("web.content_helper")
local leases_helper = require("ethernet-static-leases_helper")
local proxy = require("datamodel")
local proxy_helper = require("proxy_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local uinetwork = require("web.uinetwork_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local gOV = post_helper.getOptionalValidation
local vSIIP4 = post_helper.validateStringIsIPv4
local vSIIP6 = post_helper.validateStringIsIPv6
local vSIP = post_helper.validateStringIsPort
local vB = post_helper.validateBoolean

--region host names
local hosts_ac = uinetwork.getAutocompleteHostsList()
local mac_list,hosts_mac = leases_helper.get_mac_list(hosts_ac)
local hostname_map = {}
for k=1,(#mac_list-1) do
  local mac = mac_list[k][1]
  local hostname = mac_list[k][3]
  hostname_map[mac] = hostname
end
--endregion

local status = {
  enabledv4 = "uci.dns_hijacking.config.enabled",
  enabledv6 = "uci.tproxy.rule.@dnsv6.enabled",
}
content_helper.getExactContent(status)

local mapParams = {
  enabledv4 = "uci.dns_hijacking.config.enabled",
  dest_ipv4 = "uci.dns_hijacking.config.dest_ip",
  dest_portv4 = "uci.dns_hijacking.config.dest_port",
  enabledv6 = "uci.tproxy.rule.@dnsv6.enabled",
  dest_ipv6 = "uci.tproxy.rule.@dnsv6.destIP",
  dest_portv6 = "uci.tproxy.rule.@dnsv6.targetPort",
  verbosev6 = "uci.tproxy.config.verbose",
}
local mapValid = {
  enabledv4 = vB,
  dest_ipv4 = gOV(vSIIP4),
  dest_portv4 = gOV(vSIP),
  enabledv6 = vB,
  dest_ipv6 = gOV(vSIIP6),
  dest_portv6 = gOV(vSIP),
  verbosev6 = vB
}
local content,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if not helpmsg then
  helpmsg = {}
end

if (status.enabledv4 == "0" and content.enabledv4 == "1") or (status.enabledv6 == "0" and content.enabledv6 == "1") then
  proxy_helper.set("rpc.gui.firewall.dns_intercept.populate_ipsets","1")
end

local ip6addr = proxy.get("rpc.network.interface.@lan.ip6addr")[1].value

local function mac2hostnameFilter(data)
  data.hostname = hostname_map[untaint(data.mac)] or ""
  return true
end
local macvalidation = {
  excluded_mac_address = leases_helper.validateStaticLeaseMAC,
}
local maccols = {
  {
    header = T"MAC Address Excluded from Hijacking",
    name = "excluded_mac_address",
    param = "mac",
    type = "select",
    values = mac_list,
    attr = { select = { class="span3 no-save",maxlength="17",id="excluded_mac_address" } },
    unique = true
  },
  {
    header = T"Host Name",
    param = "hostname",
    readonly = true,
  },
}
if #mac_list <= 1 then
  maccols[1].type = "text"
  maccols[1].attr = { input = { class="span3 no-save",maxlength="17" }, autocomplete = hosts_mac }
end
local macopts = {
  tableid = "excluded_macs",
  basepath = "rpc.gui.firewall.dns_intercept.exceptions.@.",
  canEdit = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add Excluded MAC Address",
  onAdd = function(index,record)
    proxy_helper.set("rpc.gui.firewall.dns_intercept.exceptions."..index..".mac",record.excluded_mac_address)
  end}
local macexcdata,macexchelpmsg = post_helper.handleTableQuery(maccols,macopts,mac2hostnameFilter,nil,macvalidation)

local enabled_attr = {
  input = { class = "monitor-changes" }
}
local monitor_attr = {
  group = { class = "monitor-enabledv4 monitor-1" },
  input = { class = "span2", placeholder = "Leave blank for Device" },
  span = { class = "span7", },
}
local monitor_port_attr = {
  group = { class = "monitor-enabledv4 monitor-1" },
  input = { type = "number", min = "0", max = "65535", style = "width:100px",},
}
local monitor6_attr = {
  group = { class = "monitor-enabledv6 monitor-1" },
  input = { class = "span3", placeholder = "Leave blank for Device" },
  span = { class = "span7", },
}
local monitor6_port_attr = {
  group = { class = "monitor-enabledv6 monitor-1" },
  input = { type = "number", min = "0", max = "65535", style = "width:100px",},
}

ngx.print(ui_helper.createHeader(T"DNS",true,false),'\
<div class="modal-body update">\
  '); lp.include("tabs-dns.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/dns-hijacking-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <legend>',T"DNS Hijacking",'</legend>',
      ui_helper.createLabel(T"When Enabled",T"<ul>\
        <li>Plain DNS will be hijacked and forwarded to the specified DNS server</li>\
        <li>Plain DNS will be <b>NOT</b> be hijacked if destined for the specified DNS Server or one defined on the Local Network screens</li>\
        <li>DNS-over-HTTPS, DNS-over-TLS and DNS-over-QUIC will be <b>BLOCKED</b></li>\
        <li>Excluded MAC Addresses will have <b>NO</b> DNS requests hijacked or blocked</li>\
        </ul>",{span = { class = "span7", }}),'\
    <legend><small>',T"IPv4",'</small></legend>',
      ui_helper.createSwitch(T"Enabled","enabledv4",content.enabledv4,enabled_attr),
      ui_helper.createInputText(T"DNS Server Address<span class='icon-question-sign' title='IPv4 address of the DNS server to which hijacked requests will be sent. Leave blank to have this device handle IPv4 DNS.'></span>","dest_ipv4",content.dest_ipv4,monitor_attr,helpmsg["dest_ipv4"]),
      ui_helper.createInputText(T"DNS Server Port<span class='icon-question-sign' title='Leave blank unless the DNS Server is listening on a non-standard port.'></span>","dest_portv4",content.dest_portv4,monitor_port_attr,helpmsg["dest_port"]),'\
    <legend><small>',T"IPv6",'</small></legend>');
    if ip6addr == "" then
      ngx.print(
        ui_helper.createLabel(T"Disabled","No IPv6 LAN address"),
        ui_helper.createSimpleInputHidden("enabledv6",content.enabledv6),
        ui_helper.createSimpleInputHidden("dest_ipv6",""))
    else
      ngx.print(
        ui_helper.createSwitch(T"Enabled","enabledv6",content.enabledv6,enabled_attr),
        ui_helper.createInputText(T"DNS Server Address<span class='icon-question-sign' title='IPv6 address of the DNS server to which hijacked requests will be sent. Leave blank to have this device handle IPv6 DNS.'></span>","dest_ipv6",content.dest_ipv6,monitor6_attr,helpmsg["dest_ipv6"]),
        ui_helper.createInputText(T"DNS Server Port<span class='icon-question-sign' title='Leave blank unless the DNS Server is listening on a non-standard port.'></span>","dest_portv6",content.dest_portv6,monitor6_port_attr,helpmsg["targetPort"]),
        ui_helper.createSwitch(T"Verbose Logging","verbose",content.verbosev6,monitor6_attr,helpmsg["verbose"]))
    end
    ngx.print('\
      <legend>',T"Excluded MAC Addresses",'</legend>',
      ui_helper.createTable(maccols,macexcdata,macopts,nil,macexchelpmsg),'\
  </form>\
  <script src="/js/modal-busy.js"></script>\
</div>');
ngx.print('<script>\
$("#excluded_mac_address").change(function () {\
  if ((this.value) == "custom") {\
    $(this).replaceWith($("<input/>",{"type":"text","name":this.name,"class":this.className}));\
  }\
});\
$("#excluded_macs>tbody>tr:not(.line-edit)>td:nth-child(1)").each(function(){\
  $(this).text(function(i,content){\
    return content.split(" ")[0];\
  });\
});\
</script>',ui_helper.createFooter());