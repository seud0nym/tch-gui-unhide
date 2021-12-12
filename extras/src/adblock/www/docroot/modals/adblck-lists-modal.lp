--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-code')

local post_helper = require("web.post_helper")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local proxy = require("datamodel")

local qry_result = "<style>.whitelist_cb{margin-bottom:5px}</style>"
if ngx.req.get_method() == "POST" then
  local format, untaint = string.format, string.untaint
  local args = ngx.req.get_post_args()

  local query = untaint(args.check_filtered)
  if query and query ~= "" then
    local _,result,host = proxy.set("rpc.gui.adblock.query", query)
    for host in string.gmatch(untaint(result[1]["errmsg"])," %+ (%S+)") do
      qry_result = format("%s<br><input type='checkbox' class='whitelist_cb' name='wl_host' value='%s' title='Tick to whitelist %s'>%s",qry_result,host,host,host)
    end
  end

  local whitelist = untaint(args.wl_host)
  if whitelist then
    local host
    if type(whitelist) == "string" or type(whitelist) == "userdata" then
      whitelist = { whitelist }
    end
    for _,host in pairs(whitelist) do
      local key, errors = proxy.add("rpc.gui.adblock.whitelist.")
      if not key then
        local err
        for _,err in ipairs(errors) do
          message_helper.pushMessage(T(string.format("Failed to add %s to whitelist: %s (%s)",host,err.errmsg,err.errcode)), "error")
        end
      else
        local path = "rpc.gui.adblock.whitelist.".. key .. ".host";
        local result, errors = proxy.set(path, host)
        if result then
          message_helper.pushMessage(T("Added "..host.." to whitelist"), "success")
        else
          local err
          for _,err in ipairs(errors) do
            message_helper.pushMessage(T(string.format("Failed to set %s to '%s': %s (%s)",err.path,host,err.errmsg,err.errcode)), "error")
          end
        end
      end
    end
  end
end

local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString

local content = {
  status = "rpc.gui.adblock.status",
  blacklist_enabled = "uci.adblock.source.@blacklist.enabled",
}
content_helper.getExactContent(content)

local editable = content.status == "enabled"

local list_valid = {
  domain = vNES
}

local list_columns = {
  {
    header = T"Host Name",
    name = "host",
    param = "host",
    type = "text",
    attr = { input = { class="span2",maxlength="127" } }
  },
}

local blacklist_options = {
  tableid = "adb_blacklist",
  basepath = "rpc.gui.adblock.blacklist.@.",
  canAdd = editable,
  canDelete = editable,
  canEdit = editable,
  createMsg = T"Add black-listed host",
  onAdd = function(index, content)
    proxy.set("rpc.gui.adblock.blacklist."..index..".host", content.host)
  end
}

local whitelist_options = {
  tableid = "adb_whitelist",
  basepath = "rpc.gui.adblock.whitelist.@.",
  canAdd = editable,
  canDelete = editable,
  canEdit = editable,
  createMsg = T"Add white-listed host",
  onAdd = function(index, content)
    proxy.set("rpc.gui.adblock.whitelist."..index..".host", content.host)
  end
}

local blacklist_data,blacklist_helpmsg = post_helper.handleTableQuery(list_columns,blacklist_options,nil,nil,list_valid)
local whitelist_data,whitelist_helpmsg = post_helper.handleTableQuery(list_columns,whitelist_options,nil,nil,list_valid)

local source_valid = {
  enabled = vB
}

local source_columns = {
  {
    header = T"Name",
    name = "paramindex",
    param = "paramindex",
    type = "text",
    readonly = true,
  },
  {
    header = T"Source",
    name = "adb_src",
    param = "adb_src",
    type = "text",
    readonly = true,
  },
  {
    header = T"Description",
    name = "adb_src_desc",
    param = "adb_src_desc",
    type = "text",
    readonly = true,
  },
  {
    header = T"Enabled",
    name = "enabled",
    param = "enabled",
    type = "switch",
  },
}

local source_options = {
  tableid = "adb_sources",
  basepath = "uci.adblock.source.@.",
  canAdd = false,
  canDelete = false,
  canEdit = editable,
  sorted = "paramindex",
}

local source_data,source_helpmsg = post_helper.handleTableQuery(source_columns,source_options,nil,nil,source_valid)

local warn_box = {
  alert = {
    class = "alert alert-warn",
    style = "text-align:center",
  }
}

ngx.print('\
');  ngx.print(ui_helper.createHeader("Adblock",false,true) ); ngx.print('\
<div class="modal-body update">\
  <ul class="nav nav-tabs">\
    <li><a href="#" data-remote="/modals/adblck-config-modal.lp">Configuration</a></li>\
    <li class="active"><a href="#" data-remote="/modals/adblck-lists-modal.lp">Lists</a></li>\
  </ul>\
  <form class="form-horizontal" method="post" action="modals/adblck-lists-modal.lp">\
    '); ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
    ');
      local html = {}
      if editable then
        html[#html+1] = "</fieldset>"
        html[#html+1] = "<fieldset><legend>" .. T"Check Filtering" .. "</legend>"
        html[#html+1] = ui_helper.createInputText("Host or Domain Name", "check_filtered","")
        html[#html+1] = "<pre>"
        html[#html+1] = qry_result
        html[#html+1] = "</pre>"
        html[#html+1] = "</fieldset>"
      else
        html[#html + 1] = ui_helper.createAlertBlock(T("Adblock status is currently '"..content.status.."'. Status must be 'enabled' for querying and editing."), warn_box)
      end
      if content.blacklist_enabled == "1" then
        html[#html+1] = "<fieldset><legend>" .. T"Custom Black List" .. "</legend>"
        html[#html+1] = ui_helper.createTable(list_columns,blacklist_data,blacklist_options,nil,blacklist_helpmsg)
        html[#html+1] = "</fieldset>"
      end
      html[#html+1] = "<fieldset><legend>" .. T"Custom White List" .. "</legend>"
      html[#html+1] = ui_helper.createTable(list_columns,whitelist_data,whitelist_options,nil,whitelist_helpmsg)
      html[#html+1] = "</fieldset>"
      html[#html+1] = "<fieldset><legend>" .. T"Sources" .. "</legend>"
      if editable then
        html[#html + 1] = ui_helper.createAlertBlock(T"Do not enable too many lists, or you could run out or memory!", warn_box)
      end
      html[#html+1] = ui_helper.createTable(source_columns,source_data,source_options,nil,source_helpmsg)
      html[#html+1] = "</fieldset>"
      ngx.print(html)
      ngx.print('\
  </form>\
</div>\
'); ngx.print(ui_helper.createFooter()); ngx.print('\
<script>\
  $(".whitelist_cb").on("change", function(){\
    $("#modal-no-change").addClass("hide");\
    $("#modal-changes").removeClass("hide");\
  });\
</script>\
');

