--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local isBridgedMode = require("bridgedmode_helper").isBridgedMode()
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local vlan_helper = require("ethernet-vlan_helper")

local ngx = ngx
local session = ngx.ctx.session
local concat = table.concat
local find,gmatch,match,sub = string.find,string.gmatch,string.match,string.sub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local vB = post_helper.validateBoolean
local gVIES = post_helper.getValidateInEnumSelect
local gVNIR = post_helper.getValidateNumberInRange

local cpu_port = vlan_helper.get_cpu_port()
local untagged_flag = vlan_helper.get_untagged_flag()

local enabled_was = proxy.get(vlan_helper.get_switch_path("enable_vlan"))
if not enabled_was then
  enabled_was = {{value="0",missing=true}}
end

local tagged_state = {
  { "X",T"Off" },
  { untagged_flag,T"Untagged" },
  { "t",T"Tagged" },
}

local tagged_state_attr = {select={class="span2 no-save",style="font-family:FontAwesome,Roboto,Noto,sans-serif;"}}
local eth = vlan_helper.get_port_states()

local vlan_columns = {
  {
    header = T"VLAN ID",
    name = "vlan",
    param = "vlan",
    type = "text",
    attr = { input = { type="number",max="4095",min="2",step="1",class="span2 no-save" } },
    unique = true,
  },
  {
    header = concat(ui_helper.createSwitchPort("1",eth.state_0,eth.speed_0)),
    name = "p0",
    type = "select",
    values = tagged_state,
    attr = tagged_state_attr,
  },
  {
    header = concat(ui_helper.createSwitchPort("2",eth.state_1,eth.speed_1)),
    name = "p1",
    type = "select",
    values = tagged_state,
    attr = tagged_state_attr,
  },
  {
    header = concat(ui_helper.createSwitchPort("3",eth.state_2,eth.speed_2)),
    name = "p2",
    type = "select",
    values = tagged_state,
    attr = tagged_state_attr,
  },
  {
    header = concat(ui_helper.createSwitchPort("4",eth.state_3,eth.speed_3)),
    name = "p3",
    type = "select",
    values = tagged_state,
    attr = tagged_state_attr,
  },
  {
    header = "Used By",
    name = "usage",
    type = "text",
    readonly = true,
  },
  {
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

local check_range = gVNIR(2,4095)
local check_state = gVIES(tagged_state)
local check_vlans = vlan_helper.get_switch_vlans(isBridgedMode)

local function validatePort(value,object,key)
  local valid,errmsg = check_state(value)
  if valid and value == untagged_flag then
    local port = match(key,"p(%d)")
    local base = "eth"..port
    local vlan = object.vlan
    for k=1,#check_vlans do
      local v = check_vlans[k]
      if v.vlan ~= vlan then
        if v[base] and v[base] ~= "t" then
          valid = nil
          errmsg = string.format("Port %d is already untagged in VLAN %s",tonumber(port)+1,v.vlan)
          break
        end
      end
    end
  end
  return valid,errmsg
end

local function validateVLANID(value)
  local post_data = ngx.req.get_post_args(0)
  if post_data.action == "TABLE-MODIFY" and not post_data.vlan then
    return true
  end
  return check_range(value)
end

local vlan_valid = {
  vlan = validateVLANID,
  p0 = validatePort,
  p1 = validatePort,
  p2 = validatePort,
  p3 = validatePort,
}

local content,content_helpmsg
local vlan_data,vlan_helpmsg
local deleted_vlan_id

if ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args(0)
  if post_data.tableid == "vlans" and post_data.action == "TABLE-EDIT" then
    vlan_columns[vlan_column_index.vlan].readonly = true
  end
  if enabled_was[1].value == "0" then
    local enabled = post_data["enabled"]
    if enabled == "1" and enabled_was[1].value == "0" then
      vlan_helper.add_switch(post_data["jumbo"] or "0")
    end
  end
  if post_data.action == "TABLE-DELETE" then
    local vlan_ids = session:retrieve("vlan_data_index") or {}
    local deleted = tonumber(post_data.index) or -1
    deleted_vlan_id = untaint(vlan_ids[deleted])
  end
end

local vlan_options = {
  canAdd = true,
  canDelete = true,
  canEdit = true,
  createMsg = T"Add VLAN",
  tableid = "vlans",
  basepath = "uci.network.switch_vlan.@.",
  sorted = function (a,b)
    return (tonumber(a.vlan) or -1) < (tonumber(b.vlan) or -1)
  end,
}

if enabled_was[1].missing and ngx.var.request_method ~= "POST" then
  content = {
    enabled = "0",
    jumbo = "0",
  }
  vlan_data,vlan_helpmsg = {},{}
else
  local mapParams = {
    enabled = vlan_helper.get_switch_path("enable_vlan"),
    jumbo = vlan_helper.get_switch_path("jumbo"),
  }
  local mapValid = {
    enabled = vB,
    jumbo = vB,
  }
  content,content_helpmsg = post_helper.handleQuery(mapParams,mapValid)

  local vlan_usage,vlan_ids = vlan_helper.fix_ifnames(enabled_was,content.enabled)

  local function vlan_filter(data)
    if data.device == vlan_helper.get_switch_device() then
      local vlan_id = untaint(data.vlan)
      if vlan_usage[vlan_id] and vlan_usage[vlan_id].count > 0 then
        return { canDelete = false, canEdit = true }
      end
      return true
    end
    return false
  end

  vlan_options.onAdd = vlan_helper.onAdd()
  vlan_options.onDelete = vlan_helper.onDelete(deleted_vlan_id)
  vlan_options.onModify = vlan_helper.onModify(vlan_usage)

  vlan_data,vlan_helpmsg = post_helper.handleTableQuery(vlan_columns,vlan_options,vlan_filter,nil,vlan_valid)
  local tablesessionindexes = vlan_options.tableid .. ".allowedindexes"
  local allowedIndexes = session:retrieve(tablesessionindexes) or {}
  for _,v in ipairs(vlan_data) do
    vlan_ids[untaint(v[vlan_column_index.vlan])] = nil
  end
  for k,v in pairs(vlan_ids) do
    if v then
      local row = {}
      for i,c in ipairs(vlan_columns) do
        if c.name == "vlan" then
          row[i] = k
        elseif c.name == "ports" then
          row[i] = ""
          for p,ifname in pairs(vlan_usage[k].ports) do
            if ifname and sub(ifname,5,5) == "." then
              row[i] = (row[i] ~= "" and row[i].." " or "")..p.."t"
            end
          end
        else
          row[i] = ""
        end
      end
      vlan_data[#vlan_data+1] = row
      allowedIndexes[#allowedIndexes+1] = { canDelete = true, canEdit = false }
    end
  end
  session:store(tablesessionindexes,allowedIndexes)

  local vlan_data_index = {}
  for k=1,#vlan_data do
    vlan_data_index[k] = vlan_data[k][vlan_column_index.vlan]
  end
  session:store("vlan_data_index",vlan_data_index)

  local port_state_pattern = find("().%+-*?[^$",untagged_flag,nil,true) and "(%d)([%"..untagged_flag.."t]*)" or "(%d)(["..untagged_flag.."t]*)"
  for k=1,#vlan_data do
    local v = vlan_data[k]
    local vlan_id = untaint(v[vlan_column_index.vlan])
    local ports = untaint(v[vlan_column_index.ports])
    if vlan_usage[vlan_id] then
      v[vlan_column_index.usage] = vlan_usage[vlan_id].interfaces
    end
    for port=0,3,1 do
      v[vlan_column_index.p0+port] = "X"
    end
    for p in gmatch(ports,"(%S+)") do
      if p ~= cpu_port then
        local port,state = match(p,port_state_pattern)
        if state then
          v[vlan_column_index.p0+port] = state
        end
      end
    end
  end
end
if not content_helpmsg then
  content_helpmsg = {}
end

local system_vlan_data = {}
local system_vlan_options = {}
local switch_vlan,_,system_vlans = vlan_helper.get_switch_vlans(isBridgedMode)
if system_vlans then
  for i=1,#switch_vlan do
    local type = switch_vlan[i].type
    if type then
      local v = {}
      v[vlan_column_index.vlan] = switch_vlan[i].vlan
      for port=0,3,1 do
        v[vlan_column_index.p0+port] = "t"
      end
      v[vlan_column_index.usage] = (type == "guest") and "Guest" or "lan"
      system_vlan_data[#system_vlan_data+1] = v
    end
  end
  if #system_vlan_data then
    system_vlan_options = {
      canAdd = false,
      canDelete = false,
      canEdit = false,
      tableid = "sysvlans",
      sorted = function (a,b)
        return (tonumber(a.vlan) or -1) < (tonumber(b.vlan) or -1)
      end,
    }
  end
end

local switch_class_enable = { input = { class = "monitor-changes", } }
local enabled_visible_attr = { group = { class = "monitor-enabled monitor-1", } }
local info_attr = { alert = { class = "alert-info" },}

local lp = require("web.lp")
lp.setpath("/www/snippets/")

ngx.print(ui_helper.createHeader(T"Local Network",true,false),'\
<div class="modal-body update">\
'); lp.include("tabs-lan.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="',ngx.var.request_uri,'">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>',T"Switch Configuration",'</legend>',
        ui_helper.createSwitch(T"User VLANs Enabled","enabled",content["enabled"],switch_class_enable,content_helpmsg["enabled"]))
        if content.enabled == "1" then
          ngx.print(ui_helper.createSwitch(T"Jumbo Frames","jumbo",content["jumbo"],enabled_visible_attr,content_helpmsg["jumbo"]))
        end
        if content.enabled == "1" or #system_vlan_data > 0 then
          ngx.print(ui_helper.createAlertBlock(T("The purpose of a tagged port is to pass traffic for multiple VLANs, whereas an untagged port accepts traffic for only a single VLAN. Generally speaking, tagged ports will link switches, and untagged ports will link to end devices."),info_attr),'\
          <style>\
            .socket{margin-right:6px !important;}\
            #vlans th:nth-child(1),#vlans td:nth-child(1),#sysvlans th:nth-child(1),#sysvlans td:nth-child(1){text-align:right;}\
          </style>')
        end
        if content.enabled == "1" then
          ngx.print(
            '<legend>',T"User VLANs",'</legend>',
            ui_helper.createTable(vlan_columns,vlan_data,vlan_options,enabled_visible_attr,vlan_helpmsg)
          )
        end
        if #system_vlan_data > 0 then
          ngx.print(
            '<legend>',T"System VLANs",'</legend>',
            ui_helper.createTable(vlan_columns,system_vlan_data,system_vlan_options,nil,nil)
          )
        end
        ngx.print('\
    </fieldset>\
  </form>\
  <script src="/js/modal-busy.js"></script>\
</div>\
',ui_helper.createFooter())