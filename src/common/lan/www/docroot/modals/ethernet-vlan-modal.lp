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
  enabled_was = {{value = "0", missing = true}}
end

local tagged_state = {
  { "X",T"<span class='red' style='font-family:FontAwesome;'>&#xf05e;</span>" },
  { untagged_flag,T"Untagged" },
  { "t",T"Tagged" },
}

local tagged_state_attr = { select = { class="span2", style="font-family:FontAwesome,Roboto,Noto,sans-serif;" } }
local eth = vlan_helper.get_port_states()

local vlan_columns = {
  {
    header = T"VLAN ID",
    name = "vlan",
    param = "vlan",
    type = "text",
    attr = { input = { type="number",max="4095",min="100",step="1",class="span2" } },
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
    header = concat(ui_helper.createSwitchPort("WAN",eth.state_4,eth.speed_4,{socket={class="red"}})),
    name = "p4",
    type = "select",
    values = tagged_state,
    attr = tagged_state_attr,
    readonly = not isBridgedMode,
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

local function validateVLANID(value)
  local post_data = ngx.req.get_post_args(0)
  if post_data.action == "TABLE-MODIFY" and not post_data.vlan then
    return true
  end
  local check_range = gVNIR(100,4095)
  return check_range(value)
end

local vlan_valid = {
  vlan = validateVLANID,
  p0 = gVIES(tagged_state),
  p1 = gVIES(tagged_state),
  p2 = gVIES(tagged_state),
  p3 = gVIES(tagged_state),
  p4 = gVIES(tagged_state),
}

local content,content_helpmsg
local vlan_data,vlan_helpmsg
local deleted_vlan_id

if ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args(0)
  if post_data.tableid == "vlans" and post_data.action == "TABLE-EDIT" then
    vlan_columns[vlan_column_index.vlan].readonly = true
  end
  if enabled_was[1].value == "0" and enabled_was[1].missing then
    local enabled = post_data["enabled"]
    if enabled == "1" and enabled_was[1].value == "0" and enabled_was[1].missing then
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
  canEdit = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add VLAN",
  tableid = "vlans",
  basepath = "uci.network.switch_vlan.@.",
  sorted = "vlan",
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
      if tonumber(vlan_id) <= 100 or (vlan_usage[vlan_id] and vlan_usage[vlan_id].count > 0) then
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
  for k,v in ipairs(vlan_data) do
    vlan_data_index[k] = v[vlan_column_index.vlan]
  end
  session:store("vlan_data_index",vlan_data_index)

  local port_state_pattern = find("().%+-*?[^$",untagged_flag,nil,true) and "(%d)([%"..untagged_flag.."t]*)" or "(%d)(["..untagged_flag.."t]*)"
  for _,v in ipairs(vlan_data) do
    local vlan_id = untaint(v[vlan_column_index.vlan])
    local ports = untaint(v[vlan_column_index.ports])
    if vlan_usage[vlan_id] then
      v[vlan_column_index.usage] = vlan_usage[vlan_id].interfaces
    end
    for port=0,4,1 do
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
            '<style>.socket{margin-right:6px !important;} #vlans th:nth-child(1),#vlans td:nth-child(1){text-align:right;}</style>',
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