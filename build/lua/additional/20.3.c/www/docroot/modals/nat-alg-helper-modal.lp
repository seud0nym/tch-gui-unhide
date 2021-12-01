--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require ("datamodel")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local toupper = string.upper
local vB = post_helper.validateBoolean

-- Populated by 102-NAT-ALG
local available = {}

local lan_path
local wan_path
local zones = proxy.getPN("uci.firewall.zone.",true)
for _,v in ipairs(zones) do
  local name = untaint(proxy.get(v.path.."name"))
  if name then
    local wan = proxy.get(v.path.."wan")
    if wan and wan[1].value == "1" then
      wan_path = v.path.."helper."
    elseif name[1].value == "lan" then
      lan_path = v.path.."helper."
    end
  end
end

local helper_columns = {
  {
    header = T"Name",
    name = "name",
    param = "name",
    readonly = true,
  },
  {
    header = T"Description",
    name = "description",
    param = "description",
    readonly = true,
  },
  {
    header = T"Port",
    name = "port",
    param = "port",
    readonly = true,
  },
  {
    header = T"Protocol",
    name = "proto",
    param = "proto",
    readonly = true,
  },
  {
    header = T"Family",
    name = "family",
    param = "family",
    readonly = true,
  },
  {
    header = T"Enabled",
    name = "enable",
    param = "enable",
    type = "switch",
  },
}

local function findHelperPath(path,helper)
  local helpers = proxy.get(path)
  if helpers then
    for _,h in pairs(helpers) do
      if untaint(h.value) == helper then
        return h.path
      end
    end
  end
  return nil
end

local function addHelper(zone,path,helper)
  local key,errors = proxy.add(path)
  if not key then
    for _,err in ipairs(errors) do
      message_helper.pushMessage(T(string.format("Failed to add %s helper to %s Zone: %s (%s)",helper,zone,err.errmsg,err.errcode)),"error")
    end
  else
    local newpath = path.."@"..key..".value"
    local result,errors = proxy.set(newpath,helper)
    if not result then
      for _,err in ipairs(errors) do
        message_helper.pushMessage(T(string.format("Failed to add %s helper to %s Zone: %s (%s)",helper,zone,err.errmsg,err.errcode)),"error")
      end
    end
  end
end

local function delHelper(zone,path,helper)
  local result,errors = proxy.del(path)
  if not result then
    for _,err in ipairs(errors) do
      message_helper.pushMessage(T(string.format("Failed to delete %s helper from %s Zone: %s (%s)",helper,zone,err.errmsg,err.errcode)),"error")
    end
  end
end

local helper_options = {
  tableid = "helper_list",
  basepath = "uci.firewall_helpers.helper.",
  canAdd = false,
  canDelete = false,
  canEdit = true,
  sorted = function(a,b)
    return toupper(a.name or "") < toupper(b.name or "")
  end,
  onModify = function(index,content)
    if index == nil then
      return
    end
    local name = untaint(proxy.get("uci.firewall_helpers.helper."..index..".name")[1].value)
    local lan_helper_path = findHelperPath(lan_path,name)
    local wan_helper_path = findHelperPath(wan_path,name)
    if content.enable == "1" then
      if not lan_helper_path then
        addHelper("LAN",lan_path,name)
      end
      if not wan_helper_path then
        addHelper("WAN",wan_path,name)
      end
    else
      if lan_helper_path then
        delHelper("LAN",lan_helper_path,name)
      end
      if wan_helper_path then
        delHelper("WAN",wan_helper_path,name)
      end
    end
  end,
}

local helper_filter = function(data)
  local name = untaint(data["name"])
  if not available[name] then
    return { canEdit = false }
  end
  return true
end

local helper_valid = {
  enable = vB,
}

local helper_data,helper_helpmsg = post_helper.handleTableQuery(helper_columns,helper_options,helper_filter,nil,helper_valid)

ngx.print('\
');  ngx.print(ui_helper.createHeader(T"NAT Helpers (ALG's)",false,false))   ngx.print('\
<div class="modal-body update">\
');
local card_hidden = proxy.get("uci.web.card.@card_natalghelper.hide")
if card_hidden and card_hidden[1].value == "1" then
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-firewall.lp")
end
ngx.print('\
  <form class="form-horizontal" method="post" action="modals/nat-alg-helper-modal.lp">\
    ');  
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<fieldset>');
    ngx.print('<legend>');  ngx.print( T"Helpers" ); ngx.print('</legend>');
    ngx.print(ui_helper.createTable(helper_columns,helper_data,helper_options,nil,helper_helpmsg))
    ngx.print('</fieldset>');
    ngx.print('\
  </form>\
</div>\
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
'); 