--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-code')

local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local proxy = require("datamodel")
local format,gmatch,gsub,match = string.format,string.gmatch,string.gsub,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local qry_result = { T"<style>.whitelist_cb{margin-bottom:5px}</style>" }
if ngx.req.get_method() == "POST" then
  local args = ngx.req.get_post_args()
  local apply_changes = false

  local query = untaint(args.check_filtered)
  if query and query ~= "" then
    local _,result = proxy.set("rpc.adblock.query",query)
    for line in gmatch(gsub(untaint(result[1]["errmsg"]),"set%(%) failed: ",""),"([^\n]*\n?)") do
      local host = match(line,"^%s+%+%s+([^%s%[]+)\n")
      if host then
        qry_result[#qry_result+1] = T(gsub(line,"%+",format("<input type='checkbox' class='whitelist_cb' name='wl_host' value='%s' title='Tick to whitelist %s'>",host,host)))
      else
        qry_result[#qry_result+1] = T(line)
      end
    end
  end
  local whitelist = untaint(args.wl_host)
  if whitelist then
    if type(whitelist) == "string" or type(whitelist) == "userdata" then
      whitelist = { whitelist }
    end
    for _,host in pairs(whitelist) do
      local key,add_errmsg = proxy.add("rpc.adblock.whitelist.")
      if not key then
        for _,err in ipairs(add_errmsg) do
          message_helper.pushMessage(T(format("Failed to add %s to whitelist: %s (%s)",host,err.errmsg,err.errcode)),"error")
        end
      else
        local result,set_errmsg = proxy.set("rpc.adblock.whitelist."..key..".host",host)
        if result then
          message_helper.pushMessage(T("Added "..host.." to whitelist"),"success")
          apply_changes = true
        else
          for _,err in ipairs(set_errmsg) do
            message_helper.pushMessage(T(format("Failed to set %s to '%s': %s (%s)",err.path,host,err.errmsg,err.errcode)),"error")
          end
        end
      end
    end
  end

  if apply_changes then
    proxy.apply()
  end
end

local list_valid = {
  domain = post_helper.validateNonEmptyString
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
  basepath = "rpc.adblock.blacklist.@.",
  canAdd = true,
  canDelete = true,
  canEdit = true,
  createMsg = T"Add black-listed host",
  onAdd = function(index,data)
    proxy.set("rpc.adblock.blacklist."..index..".host",data.host)
  end
}

local whitelist_options = {
  tableid = "adb_whitelist",
  basepath = "rpc.adblock.whitelist.@.",
  canAdd = true,
  canDelete = true,
  canEdit = true,
  createMsg = T"Add white-listed host",
  onAdd = function(index,data)
    proxy.set("rpc.adblock.whitelist."..index..".host",data.host)
  end
}

local blacklist_data,blacklist_helpmsg = post_helper.handleTableQuery(list_columns,blacklist_options,nil,nil,list_valid)
local whitelist_data,whitelist_helpmsg = post_helper.handleTableQuery(list_columns,whitelist_options,nil,nil,list_valid)

local content = {
  status = "rpc.adblock.status",
}
content_helper.getExactContent(content)

ngx.print(ui_helper.createHeader("Adblock",false,true),'\
<div class="modal-body update">')
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-adblock.lp")
  ngx.print('\
  <form class="form-horizontal" method="post" action="modals/adblck-lists-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset><legend>',T"Check Filtering",'</legend>')
      if content.status == "enabled" then ngx.print(
        ui_helper.createInputText("Host or Domain Name","check_filtered",""),'\
        <pre>',qry_result,'</pre>')
      else
        local warn_box = {
          alert = {
            class = "alert alert-warn",
            style = "text-align:center",
          }
        }
        if content.status == "running" or content.status == "" then
          ngx.print(ui_helper.createAlertBlock(T"Unable to run a query as Adblock is currently running a block list update. Please try again soon.",warn_box))
        else
          ngx.print(ui_helper.createAlertBlock(T(format("Unable to run a query as Adblock is currently %s. Please try again.",content.status)),warn_box))
        end
      end ngx.print('\
    </fieldset>','\
    <fieldset><legend>',T"Custom Black List",'</legend>',
      ui_helper.createTable(list_columns,blacklist_data,blacklist_options,nil,blacklist_helpmsg),'\
    </fieldset>\
    <fieldset><legend>',T"Custom White List",'</legend>',
      ui_helper.createTable(list_columns,whitelist_data,whitelist_options,nil,whitelist_helpmsg),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter(),'\
<div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>\
<script>\
  $(function(){\
    function waiting_action(self) {\
      var msg_dst = $(self);\
      var busy_msg = $(".loading-wrapper");\
      msg_dst.after(busy_msg);\
      busy_msg.removeClass("hide");\
      busy_msg[0].scrollIntoView();\
      $(".modal-body").scrollLeft(0);\
    };\
    $(document).on("click", "#save-config", function () {\
      waiting_action(this);\
    });\
  });\
  $(document).on("change",".whitelist_cb", function(){\
    $("#modal-no-change").addClass("hide");\
    $("#modal-changes").removeClass("hide");\
  });\
</script>\
')

