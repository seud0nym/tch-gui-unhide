--pretranslated: do not change this file
 
-- Localization
gettext.textdomain('webui-core')

local logger = require("tch.logger")
local log = logger.new("nat-alg-helper-modal", 7)
local ngx = ngx
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require ("datamodel")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local substring, toupper, untaint = string.sub, string.upper, string.untaint
local gVIES = post_helper.getValidateInEnumSelect

local helper_columns = {
  {
    header = T"Name",
    name = "name",
    param = "name",
    type = "text",
  },
  {
    header = T"Port",
    name = "port",
    param = "port",
  },
  {
    header = T"Protocol",
    name = "proto",
    param = "proto",
  },
  {
    header = T"Family",
    name = "family",
    param = "family",
  },
  {
    header = T"Assignable<br>to Zone",
    name = "enable",
    param = "enable",
    type = "switch",
  },
}

local helper_options = {
  tableid = "helper_list",
  basepath = "uci.firewall_helpers.helper.",
  canAdd = false,
  canDelete = false,
  canEdit = false,
  sorted = function(a, b)
    return toupper(a.name or "") < toupper(b.name or "")
  end,
}

local helper_data = content_helper.loadTableData(helper_options.basepath, helper_columns, nil, helper_options.sorted)

local helpers = {}
for k,v in pairs(helper_data) do
  if v[5] == "1" then
    helpers[#helpers+1] = { v[1], T(toupper(v[1])) }
  end
end

local lan_path
local paths = {}
local zones = proxy.getPN("uci.firewall.zone.", true)
for k,v in ipairs(zones) do
  local name = proxy.get(v.path .. "name")
  if name then
    if name[1].value == "" then
      name = "Zone " .. tostring(k)
    else
      name = untaint(name[1].value)
    end
    local wan = proxy.get(v.path .. "wan")
    if wan and wan[1].value == "1" then
      paths[#paths+1] = {
        index = tostring(k),
        name = name,
        path = v.path .. "helper.@.",
      }
    else
      if name == "lan" then
        lan_path = {
          index = tostring(k),
          name = name,
          path = v.path .. "helper.@.",
        }
      end
    end
  end
end
if lan_path then 
  paths[#paths+1] = lan_path
end

local hlp_columns = {
  {
    header = T"Protocol",
    name = "helper",
    param = "value",
    type = "select",
    values = helpers,
    attr = { select = { class="span2 centered" } },
  },
}

local hlp_valid = {
  helper = gVIES(helpers)
}

local function process(zone, data, options, helpmsg)
  local disabled = {}
  if #data == 0 then
    disabled = helpers
  elseif #data < #helpers then
    local helper
    for _,helper in pairs(helpers) do
      local found = false
      local row
      for r,row in pairs(data) do
        if row[1] == helper[1] then
          found = true
          break
        end
      end
      if not found then
        disabled[#disabled+1] = helper
      end
    end
  end

  hlp_columns[1].values = disabled

  local html = {}
  html[#html+1] ="<fieldset>"
  html[#html+1] =   "<legend>" .. T("Helpers for " .. toupper(zone) .. " Zone") .. "</legend>"
  html[#html+1] =   ui_helper.createTable(hlp_columns, data, options, nil, helpmsg)
  html[#html+1] ="</fieldset>"

  hlp_columns[1].values = helpers

  return html
end

ngx.print('\
');  ngx.print(ui_helper.createHeader(T"NAT Helpers (ALG's)", false, false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/nat-alg-helper-modal.lp">\
    ');  
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<fieldset>');
    ngx.print('<legend>');  ngx.print( T"Helpers" ); ngx.print('</legend>');
    ngx.print(ui_helper.createTable(helper_columns, helper_data, helper_options))
    ngx.print('</fieldset>');
    local alert_warn = {
      alert = {
        class = "alert alert-warn"
      },
    }
    ngx.print(ui_helper.createAlertBlock(T"NOTE: There is a known bug which prevents the last helper being removed from a zone. You can use the de-telstra script with the -an option to remove all WAN zone NAT ALG helpers.",alert_warn))

    for _,v in pairs(paths) do
      local tableid = "helper_" .. v.index
      local hlp_options = {
        tableid = tableid,
        basepath = v.path,
        canAdd = true,
        canDelete = true,
        canEdit = false,
        maxEntries = #helpers,
      }
      local basepath = substring(v.path, 1, -3)
      local hlp_data, hlp_helpmsg = post_helper.handleTableQuery(hlp_columns, hlp_options, nil, nil, hlp_valid)
      local html = process(v.name, hlp_data, hlp_options, hlp_helpmsg)
      ngx.print(html)
    end
    ngx.print('\
  </form>\
</div>\
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
'); 