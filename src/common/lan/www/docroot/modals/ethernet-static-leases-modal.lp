--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local leases_helper = require("ethernet-static-leases_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local dns_list = require('dns_servers_helper').all()
local hosts_ac = require("web.uinetwork_helper").getAutocompleteHostsList()
local mac_list,hosts_mac = leases_helper.get_mac_list(hosts_ac)
local tags = leases_helper.get_dhcp_tags()

local ngx = ngx
local ipairs,pairs,string = ipairs,pairs,string
local tonumber = tonumber
local format,lower,match = string.format,string.lower,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local vQTN = post_helper.validateQTN
local gAV = post_helper.getAndValidation
local gOV = post_helper.getOptionalValidation
local vSIIP4 = post_helper.validateStringIsIPv4

local sleases_mac_attr = { class="span2",maxlength="17",id="lease_mac",style="width:115px" }

-- Static Leases
local sleases_columns = {
  {
    header = T"MAC Address",
    name = "sleases_mac",
    param = "mac",
    type = "select",
    values = mac_list,
    attr = { select = sleases_mac_attr },
    unique = true,
  },
  {
    header = T"Host Name" ,
    name = "sleases_name",
    param = "name",
    type = "text",
    attr = { input = { class="span2",style="width:90px" } },
  },
  {
    header = T"IPv4 Address",
    name = "sleases_ip",
    param = "ip",
    type = "text",
    attr = { input = { class="span2",maxlength="15",style="width:110px" }, autocomplete = hosts_ac },
  },
  {
    header = T"Client<br>DUID&nbsp;<span class='icon-question-sign' title='DHCP Unique Identifier as 28 hexadecimal digits is required to assign the IPv6 Host ID.'></span>" ,
    name = "sleases_duid",
    param = "duid",
    type = "text",
    attr = { input = { class="span2",maxlength="28",minlength="20",style="width:210px" } },
  },
  {
    header = T"IPv6<br>Host ID" ,
    name = "sleases_hostid",
    param = "hostid",
    type = "text",
    attr = { input = { class="span2",maxlength="8",style="width:60px" } },
  },
  {
    header = T"Custom DHCP<br>Options Tag",
    name = "sleases_tag",
    param = "tag",
    type = "select",
    attr = { select = { class="span2",style="width:120px" } },
    values = tags,
  },
  {
    header = T"Add to<br>DNS&nbsp;<span class='icon-question-sign' title='Add static forward and reverse DNS entries for this host, so that the host name resolves even when lease is not active.'></span>",
    name = "sleases_dns",
    param = "dns",
    type = "switch",
    default = "1",
  },
  {
    header = "",
    name = "sleases_owner",
    param = "owner",
    type = "hidden",
    readonly = true,
  },
}
if #mac_list <= 1 then
  sleases_columns[1].type = "text"
  sleases_columns[1].attr = { input = sleases_mac_attr, autocomplete = hosts_mac }
end

local getargs = ngx.req.get_uri_args()
--region sorting
local sortcol
if (getargs.sort) then
  sortcol = untaint(getargs.sort)
else
  sortcol = "name"
end
local sortfunc
if sortcol == "name" then
  sortfunc = function(a,b)
    return lower(a.name or "") < lower(b.name or "")
  end
elseif sortcol == "mac" then
  sortfunc = function(a,b)
    return lower(a.mac or "") < lower(b.mac or "")
  end
elseif sortcol == "ip" then
  local ipv4ToNumber = function(ipv4)
    local bits = {match(untaint(ipv4) or "","(%d+)%.(%d+)%.(%d+)%.(%d+)")}
    if #bits == 4 then
      return tonumber(format("%03d%03d%03d%03d",bits[1],bits[2],bits[3],bits[4]))
    else
      return 0
    end
  end
  sortfunc = function(a,b)
    return ipv4ToNumber(a.ip) < ipv4ToNumber(b.ip)
  end
else
  sortfunc = sortcol
end
--endregion

local sleases_valid = {
  sleases_name = leases_helper.validateStaticLeaseName,
  sleases_mac = gAV(leases_helper.validateStaticLeaseMAC,vQTN),
  sleases_ip = gOV(gAV(leases_helper.validateStaticLeaseIP(),vQTN)),
  sleases_hostid = leases_helper.validateHostID,
  slease_duid = leases_helper.validateDUID,
}

local sleases_options = {
  tableid = "sleases",
  basepath = "uci.dhcp.host.@.",
  createMsg = T"Add Static Lease",
  objectName = post_helper.getRandomKey(),
  addNamedObject = true,
  onAdd = leases_helper.onLeaseChange,
  onModify = leases_helper.onLeaseChange,
  sorted = sortfunc,
}

local used_tags = {}
local syshosts_data
local sleases_filter = function(data)
  used_tags[untaint(data['tag'])] = true
  if data['owner'] == "portforward" then
    local path
    if not syshosts_data then
      syshosts_data = proxy.get("sys.hosts.host.")
    end
    if syshosts_data then
      for _,v in pairs(syshosts_data) do
        if v.value == data['mac'] then
          path = v.path
          break
        end
      end
      for _,v in pairs(syshosts_data) do
        if v.path == path and v.param == "HostName" then
          data['name'] = v.value
          break
        end
      end
    end
    return { canEdit = false,canDelete = false }
  end
  if (data['name']:find("^ReservedStatic") == 1) then
    return false
  end
  return true
end

local sleases_data,sleases_helpmsg = post_helper.handleTableQuery(sleases_columns,sleases_options,sleases_filter,nil,sleases_valid)

local tags_columns = {
  {
    header = T"Tag",
    name = "tags_name",
    param = "paramindex",
    type = "text",
    attr = { input = { class="span1" } },
  },
  {
    header = T"Network ID",
    name = "tags_networkid",
    param = "networkid",
    type = "text",
    attr = { input = { class="span1" } },
  },
  {
    header = T"IPv4 Primary DNS",
    name = "tags_dns1",
    type = "select",
    values = dns_list[1],
    attr = { input = { class="span2" } },
  },
  {
    header = T"IPv4 Secondary DNS",
    name = "tags_dns2",
    type = "select",
    values = dns_list[1],
    attr = { input = { class="span2" } },
  },
  {
    header = T"",
    name = "dhcp_option",
    param = "dhcp_option",
    type = "hidden",
    readonly = true,
  },
}

local tags_filter = function(data)
  for k,v in pairs(data) do
    if match(k,"^dhcp_option.@") and match(v,"^6,") then
      data.dhcp_option = v
    end
  end
  if used_tags[untaint(data.paramindex)] then
    return { canDelete = false }
  end
  return true
end

local tags_valid = {
  tags_name = leases_helper.validateTagName,
  tags_dns1 = gOV(vSIIP4),
  tags_dns2 = gOV(vSIIP4),
}

local tags_options = {
  tableid = "tags",
  basepath = "uci.dhcp.tag.@.",
  createMsg = T"Add Tag",
  addNamedObject = true,
  canEdit = true,
  valid = leases_helper.validateTags,
  onAdd = leases_helper.onTagAdd,
  onModify = leases_helper.onTagModify,
  sorted = leases_helper.sort_tags,
}

if ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args(0)
  if post_data.tableid == "tags" and post_data.action == "TABLE-EDIT" then
    tags_columns[1].readonly = true
  end
end

local tags_data,tags_helpmsg = post_helper.handleTableQuery(tags_columns,tags_options,tags_filter,nil,tags_valid)
for i,row in pairs(tags_data) do
  tags_data[i][3] = match(row[5],"^6,([^,]+)") or ""
  tags_data[i][4] = match(row[5],"^6,[^,]+,(.+)") or ""
end

local inUseFlag = false
if ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args(0)
  if post_data.tableid == "sleases" then
    if (post_data.action == "TABLE-MODIFY" or post_data.action == "TABLE-EDIT" or post_data.action == "TABLE-ADD") then
      local text_mac_index = tonumber(post_data.index)
      local text_mac
      if post_data.action == "TABLE-ADD" then
        text_mac = post_data["sleases_mac"]
      else
        text_mac = sleases_data[text_mac_index][1]
      end
      local flag = true
      for _,j in ipairs(mac_list) do
        if text_mac == j[1] then
          flag = false
          break
        end
      end
      if flag then
        sleases_mac_attr.value = text_mac
        sleases_columns[1].type = "text"
        sleases_columns[1].attr = { input = sleases_mac_attr, autocomplete = hosts_mac }
      end
      if post_data.action == "TABLE-MODIFY" or post_data.action == "TABLE-ADD" then
        local slease_ip = post_data.sleases_ip
        if slease_ip ~= "" then
          local slease_mac = post_data.sleases_mac
          local arranged_data = content_helper.convertResultToObject("sys.hosts.host.",syshosts_data)
          if arranged_data then
            for _,v in pairs(arranged_data) do
              if v.DhcpLeaseTime ~= "" and v.IPAddress == slease_ip and v.MACAddress ~= slease_mac then
                inUseFlag = true
                break
              end
            end
          end
        end
      end
    end
  end
end

local lp = require("web.lp")
lp.setpath("/www/snippets/")

ngx.print(ui_helper.createHeader(T"Local Network",true,false),'\
<div class="modal-body update">\
'); lp.include("tabs-lan.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/ethernet-static-leases-modal.lp?sort=',sortcol,'">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>',T"Static Leases",'</legend>\
      ');
      if inUseFlag == true then
        local confilictipchange_alert = {
          alert = {
              class = "alert",
              id = "confilictipchange-msg"
          }
        }
        ngx.print(ui_helper.createAlertBlock(T"The latest added/modified static lease is set to an IP that is already in use! Please perform a reboot to apply the changes.",confilictipchange_alert))
      end
      ngx.print(ui_helper.createTable(sleases_columns,sleases_data,sleases_options,nil,sleases_helpmsg),'\
    </fieldset>\
    <fieldset>\
      <legend>',T"Custom DHCP Options",'</legend>\
      ',ui_helper.createTable(tags_columns,tags_data,tags_options,nil,tags_helpmsg),'\
    </fieldset>\
  </form>\
</div>\
<script src="/js/modal-busy.js"></script>\
',ui_helper.createFooter(),'\
<script src="/js/ethernet-modal.js"></script>\
<script>\
var hdrs = document.getElementsByTagName("th");\
for(var i=0;i<hdrs.length;i++){\
  var text=hdrs[i].innerText;\
  var sort="";\
  var title="";\
  switch(text){\
    case "MAC Address":sort="mac";break;\
    case "Host Name":sort="name";break;\
    case "IPv4 Address":sort="ip";break;\
    default:sort="";break;\
  }\
  if (sort!="") {\
    if (title=="") {\
      title="Sort by "+text;\
    }\
    hdrs[i].innerHTML=\'<a href="#" title="\'+title+\'" onclick="return showLoadingWrapper();" data-remote="modals/ethernet-static-leases-modal.lp?sort=\'+sort+\'">\'+text+\'</a>\';\
    hdrs[i].setAttribute("class","nav");\
  }\
}\
</script>\
');
