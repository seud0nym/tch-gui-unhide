-- Localization
gettext.textdomain('webui-core')

local json = require("dkjson")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local format,tolower = string.format,string.lower
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local function extractHTML(source,target)
  if type(source) == "string" then
    target[#target+1] = source
  elseif type(source) == "userdata" then
    target[#target+1] = untaint(source)
  else
    for _,v in pairs(source) do
      extractHTML(v,target)
    end
  end
end

local function genAttrib(opkg,type)
  return {
    button = {
      id = type.."-"..opkg.paramindex,
      title = format(type,opkg.name or ""),
      class = "btn-mini btn-"..type,
      ["data-index"] = opkg.paramindex,
      ["data-name"] = opkg.name,
    }
  }
end

local function getOptions(basepath)
  return {
    tableid = "opkgs",
    basepath = basepath,
    canEdit = false,
    canAdd = false,
    canDelete = false,
    sorted = function(opkg1,opkg2)
      return tolower(opkg1.name or "") < tolower(opkg2.name or "")
    end,
  }
end

local function toHTML(opkg_table)
  local opkg_html = {}
  extractHTML(opkg_table,opkg_html)
  return {
    html = table.concat(opkg_html,"\n"),
  }
end

local opkg_columns = {
  available = {
    { -- [1]
      header = T"Package<br>Name",
      name = "name",
      param = "name",
      type = "text",
    },
    { -- [2]
      header = T"Descripion",
      name = "description",
      param = "description",
      type = "text",
    },
    { -- [3] Beware CSS applied to this column
      header = T"Version",
      name = "available_version",
      param = "available_version",
      type = "text",
    },
    { -- [4]
      header = T"<i>Install</i>",
      name = "pkg_install",
      param = "pkg_upgrade",
      type = "text",
    },
    { -- [5]
      header = T"",
      name = "warning",
      param = "warning",
      type = "text",
    },
  },
  system = {
    { -- [1]
      header = T"Package<br>Name",
      name = "name",
      param = "name",
      type = "text",
    },
    { -- [2]
      header = T"Descripion",
      name = "description",
      param = "description",
      type = "text",
    },
    { -- [3] Beware CSS applied to this column
      header = T"Installed<br>Version",
      name = "installed_version",
      param = "installed_version",
      type = "text",
    },
    { -- [4] Beware CSS applied to this column
      header = T"Available<br>Version",
      name = "available_version",
      param = "available_version",
      type = "text",
    },
    { -- [5]
      header = T"Source",
      name = "system",
      param = "system",
      type = "text",
    },
    { -- [6]
      header = T"<i>Upgrade</i>",
      name = "pkg_upgrade",
      param = "pkg_upgrade",
      type = "text",
    },
    { -- [7]
      header = T"<i>Remove</i>",
      name = "pkg_remove",
      param = "pkg_remove",
      type = "text",
    },
    { -- [8]
      header = T"",
      name = "warning",
      param = "warning",
      type = "text",
    },
  },
  user = {
    { -- [1]
      header = T"Package<br>Name",
      name = "name",
      param = "name",
      type = "text",
    },
    { -- [2]
      header = T"Descripion",
      name = "description",
      param = "description",
      type = "text",
    },
    { -- [3] Beware CSS applied to this column
      header = T"Installed<br>Version",
      name = "installed_version",
      param = "installed_version",
      type = "text",
    },
    { -- [4] Beware CSS applied to this column
      header = T"Available<br>Version",
      name = "available_version",
      param = "available_version",
      type = "text",
    },
    { -- [5]
      header = T"Installed",
      name = "installed_time",
      param = "installed_time",
      type = "text",
    },
    { -- [6]
      header = T"<i>Upgrade</i>",
      name = "pkg_upgrade",
      param = "pkg_upgrade",
      type = "text",
    },
    { -- [7]
      header = T"<i>Remove</i>",
      name = "pkg_remove",
      param = "pkg_remove",
      type = "text",
    },
    { -- [8]
      header = T"",
      name = "warning",
      param = "warning",
      type = "text",
    },
  }
}

local opkg_filter = {
  available = function(data)
    if data.warning and data.warning ~= "" then
      data.warning = "<span class='icon-warning-sign' title='"..data.warning.."'></span>"
    else
      if data.paramindex and data.name then
        data.pkg_upgrade = format("<input type='checkbox' class='opkg_cb' name='install_pkg' value='%s' title='Tick to install %s'>",data.paramindex,data.name)
      else
        data.warning = "<span class='icon-warning-sign' title='Failed to extract package name or index???'></span>"
      end
    end
    return true
  end,
  system = function(data)
    if data.warning and data.warning ~= "" then
      data.warning = "<span class='icon-warning-sign' title='"..data.warning.."'></span>"
    else
      if data.available_version ~= "" then
        data.pkg_upgrade = ui_helper.createSimpleButton("","icon-plus-sign",genAttrib(data,"Upgrade %s (This is very,very risky!)"))
      end
      data.pkg_remove = ui_helper.createSimpleButton("","icon-remove",genAttrib(data,"Remove %s (This is very,very risky!)"))
    end
    return true
  end,
  user = function(data)
    if data.warning and data.warning ~= "" then
      data.warning = "<span class='icon-warning-sign' title='"..data.warning.."'></span>"
    else
      if data.paramindex and data.name then
        if data.available_version ~= "" then
          data.pkg_upgrade = format("<input type='checkbox' class='opkg_cb' name='upgrade_pkg' value='%s' title='Tick to upgrade %s'>",data.paramindex,data.name)
        end
        data.pkg_remove = format("<input type='checkbox' class='opkg_cb' name='remove_pkg' value='%s' title='Tick to remove %s'>",data.paramindex,data.name)
      else
        data.warning = "<span class='icon-warning-sign' title='Failed to extract package name or index???'></span>"
      end
    end
    if data.installed_time and data.installed_time ~= "" then
      data.installed_time = os.date("%d/%m/%Y %T",tonumber(data.installed_time))
    end
    return true
  end,
}

local getargs = ngx.req.get_uri_args()
local group
if getargs and getargs.group then
  group = untaint(getargs.group)
else
  group = "user"
end

local opkg_opts = getOptions("rpc.gui.opkg."..group..".@.")
local opkg_data = content_helper.loadTableData(opkg_opts.basepath,opkg_columns[group],opkg_filter[group],opkg_opts.sorted)
local opkg_tble = ui_helper.createTable(opkg_columns[group],opkg_data,opkg_opts)

local buffer = {}
if json.encode (toHTML(opkg_tble),{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
