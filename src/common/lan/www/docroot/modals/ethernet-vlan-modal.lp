--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local vlan_helper = require("ethernet-vlan_helper")

-- cpu_port and untagged_flag are updated by 165-VLAN
local cpu_port = "8t"
local untagged_flag = "*"

local ngx = ngx
local concat = table.concat
local find,format,gmatch,gsub,match = string.find,string.format,string.gmatch,string.gsub,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local vB = post_helper.validateBoolean
local gVIES = post_helper.getValidateInEnumSelect
local gVNIR = post_helper.getValidateNumberInRange

local port_state = {
  { "", T"<span class='red' style='font-family:FontAwesome;'>&#xf05e;</span>" },
  { untagged_flag,T"Untagged" },
  { "t",T"Tagged" },
}

local port_select_attr = { select = { class="span2", style="font-family:FontAwesome,Roboto,Noto,sans-serif;" } }
local eth = vlan_helper.get_port_states()

local vlan_columns = {
  { -- 1
    header = T"VLAN ID",
    name = "vlan",
    param = "vlan",
    type = "text",
    attr = { input = { type="number",max="4095",min="100",step="1",class="span2" } },
    unique = true,
  },
  { -- 2
    header = concat(ui_helper.createSwitchPort("1",eth.state_0,eth.speed_0)),
    name = "p0",
    type = "select",
    values = port_state,
    attr = port_select_attr,
  },
  { -- 3
    header = concat(ui_helper.createSwitchPort("2",eth.state_1,eth.speed_1)),
    name = "p1",
    type = "select",
    values = port_state,
    attr = port_select_attr,
  },
  { -- 4
    header = concat(ui_helper.createSwitchPort("3",eth.state_2,eth.speed_2)),
    name = "p2",
    type = "select",
    values = port_state,
    attr = port_select_attr,
  },
  { -- 5
    header = concat(ui_helper.createSwitchPort("4",eth.state_3,eth.speed_3)),
    name = "p3",
    type = "select",
    values = port_state,
    attr = port_select_attr,
  },
  { -- 6
    header = "Used By",
    name = "usage",
    type = "text",
    readonly = true,
  },
  { -- 7
    header = "",
    name = "ports",
    param = "ports",
    type = "hidden",
    readonly = true,
  },
}
local vlan_column_index = {}
for k,v in ipairs(vlan_columns) do
  vlan_column_index[v.name] = k
end

if ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args(0)
  if post_data.tableid == "vlans" and post_data.action == "TABLE-EDIT" then
    vlan_columns[1].readonly = true
  end
end

local function validateVLANID(value)
  local post_data = ngx.req.get_post_args(0)
  if post_data.action == "TABLE-MODIFY" and not post_data.vlan then
    return true
  end
  local check_range = gVNIR(100,4095)
  return check_range(value)
end

local function p2ports(values)
  local ports = cpu_port
  for p=3,0,-1 do
    local value = values["p"..p]
    if value ~= "" then
      ports = p..untaint(value).." "..ports
    end
  end
  return ports
end

local vlan_valid = {
  vlan = validateVLANID,
  p0 = gVIES(port_state),
  p1 = gVIES(port_state),
  p2 = gVIES(port_state),
  p3 = gVIES(port_state),
}

local vlan_options = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add VLAN",
  tableid = "vlans",
  basepath = "uci.network.switch_vlan.@.",
  sorted = "vlan",
  onAdd = function(_,values)
    local vlans = proxy.getPN("uci.network.switch_vlan.",true) or {}
    for i=#vlans,1,-1 do
      local device = proxy.get(vlans[i].path.."device")
      if device and device[1].value == "" then
        local index = match(untaint(vlans[i].path),"@([^%.]+)")
        if index then
          vlan_helper.add_vlan(index,untaint(values.vlan),p2ports(values),cpu_port)
          proxy.apply()
          break
        end
      end
    end
  end,
  onDelete = function(index)
    local vlan = proxy.get("uci.network.switch_vlan.@"..index..".vlan")
    if vlan then
      local vid = untaint(vlan[1].value)
      if vid ~= "" then
        for p=0,3,1 do
          proxy.del(format("uci.network.device.@eth%s_%s",p,vid)) -- old format
          proxy.del(format("uci.network.device.@vlan_eth%s_%s",p,vid))
        end
        proxy.apply()
      end
    end
  end,
  onModify = function(index,values)
    local ports = p2ports(values)
    if values.ports ~= ports then
      proxy.set("uci.network.switch_vlan.@"..index..".ports",ports)
      proxy.apply()
    end
  end,
}

local enabled_was = proxy.get("uci.network.switch.@bcmsw_ext.enable_vlan")
if not enabled_was then
  enabled_was = {{value = "0", missing = true}}
  if ngx.var.request_method == "POST" then
    local post_data = ngx.req.get_post_args(0)
    local enabled = post_data["enabled"]
    if enabled == "1" then
      proxy.add("uci.network.switch.","bcmsw_ext")
      proxy.set("uci.network.switch.@bcmsw_ext.name","bcmsw_ext")
      proxy.set("uci.network.switch.@bcmsw_ext.enable_vlan","1")
      proxy.set("uci.network.switch.@bcmsw_ext.jumbo",post_data["jumbo"] or "0")
      proxy.set("uci.network.switch.@bcmsw_ext.qosimppauseenable","0")
      proxy.set("uci.network.switch.@bcmsw_ext.reset","1")
      proxy.set("uci.network.switch.@bcmsw_ext.type","bcmsw")
      proxy.set("uci.network.switch.@bcmsw_ext.unit","1")
      local added,errmsg = proxy.add("uci.network.switch_vlan.")
      if added then
        vlan_helper.add_vlan(added,"1","0"..untagged_flag.." 1"..untagged_flag.." 2"..untagged_flag.." 3"..untagged_flag.." "..cpu_port,cpu_port)
      else
        message_helper.pushMessage("Error creating VLAN ID 1: "..errmsg,"error")
      end
      proxy.apply()
    end
  end
end

local content,content_helpmsg
local vlan_data,vlan_helpmsg

if enabled_was[1].missing and ngx.var.request_method ~= "POST" then
  content = {
    enabled = "0",
    jumbo = "0",
  }
  vlan_data,vlan_helpmsg = {},{}
else
  local mapParams = {
    enabled = "uci.network.switch.@bcmsw_ext.enable_vlan",
    jumbo = "uci.network.switch.@bcmsw_ext.jumbo",
  }
  local mapValid = {
    enabled = vB,
    jumbo = vB,
  }
  content,content_helpmsg = post_helper.handleQuery(mapParams,mapValid)

  local vlan_usage = vlan_helper.fix_ifnames(enabled_was,content.enabled)
  local function vlan_filter(data)
    if data.device == "bcmsw_ext" then
      local vlan_id = untaint(data.vlan)
      if tonumber(vlan_id) <= 100 or (vlan_usage[vlan_id] and vlan_usage[vlan_id].count > 0) then
        return { canDelete = false, canEdit = true }
      end
      return true
    end
    return false
  end

  vlan_data,vlan_helpmsg = post_helper.handleTableQuery(vlan_columns,vlan_options,vlan_filter,nil,vlan_valid)

  local port_state_pattern = find("().%+-*?[^$",untagged_flag,nil,true) and "(%d)([%"..untagged_flag.."t]*)" or "(%d)(["..untagged_flag.."t]*)"
  for k,v in ipairs(vlan_data) do
    local vlan_id = untaint(v[vlan_column_index.vlan])
    local ports = untaint(v[vlan_column_index.ports])
    if vlan_usage[vlan_id] then
      v[vlan_column_index.usage] = vlan_usage[vlan_id].interfaces
      if ngx.var.request_method == "POST" then
        local post_data = ngx.req.get_post_args(0)
        if post_data.action == "TABLE-MODIFY" and post_data.tableid == vlan_options.tableid and tonumber(post_data.index) == k then
          for ifname in gmatch(vlan_usage[vlan_id].interfaces, "(%S+)") do
            local ifpath = "uci.network.interface.@"..ifname..".ifname"
            local ifnames = untaint(proxy.get(ifpath)[1].value)
            local updated_ifnames = ifnames
            for p,interface in pairs(vlan_usage[vlan_id].ports) do
              if not find(ports,p,nil,true) then
                updated_ifnames = gsub(updated_ifnames,interface,"")
              end
            end
            if ifnames ~= updated_ifnames then
              updated_ifnames = gsub(gsub(gsub(updated_ifnames,"^ +","")," +$",""),"  "," ")
              proxy.set(ifpath,updated_ifnames)
              proxy.apply()
            end
          end
        end
      end
    end
    for p in gmatch(ports,"(%S+)") do
      if p ~= cpu_port then
        local port,state = match(p,port_state_pattern)
        v[vlan_column_index.p0+port] = state
      end
    end
  end
end
if not content_helpmsg then
  content_helpmsg = {}
end

local switch_class_enable = { input = { class = "monitor-changes", } }
local enabled_visible_attr = { group = { class = "monitor-enabled monitor-1", } }

local lp = require("web.lp")
lp.setpath("/www/snippets/")

ngx.print(ui_helper.createHeader(T"Local Network",true,false),'\
<div class="modal-body update">\
'); lp.include("tabs-lan.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="',ngx.var.request_uri,'">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>',T"Switch Configuration",'</legend>',
        ui_helper.createSwitch(T"VLANs Enabled","enabled",content["enabled"],switch_class_enable,content_helpmsg["enabled"]),
        ui_helper.createSwitch(T"Jumbo Frames","jumbo",content["jumbo"],enabled_visible_attr,content_helpmsg["jumbo"]))
        if content.enabled == "1" then
          ngx.print(
            '<style>#vlans th:nth-child(1),#vlans td:nth-child(1){text-align:right;}</style>',
            '<legend>',T"VLANs",'</legend>',
            ui_helper.createTable(vlan_columns,vlan_data,vlan_options,enabled_visible_attr,vlan_helpmsg)
          )
        end
        ngx.print('\
    </fieldset>\
  </form>\
  <script src="/js/modal-busy.js"></script>\
</div>\
',ui_helper.createFooter())