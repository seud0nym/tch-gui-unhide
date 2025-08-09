--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-code')

local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local proxy = require("datamodel")
local format,match,sub = string.format,string.match,string.sub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local category = { ["1hosts"] = "hst", ["hagezi"] = "hag", ["stevenblack"] = "stb", ["utcapitole"] = "utc"}

if ngx.req.get_method() == "POST" then
  local args = ngx.req.get_post_args()
  local apply_changes = false

  for k,v in pairs(args) do
    local name = untaint(k)
    if name == "adb_sources" or match(name,"^adb_..._sources$") then
      local selected = type(v) == "table" and v or {v}
      local path = "uci.adblock.global."..name.."."
      local retval = proxy.del(path.."@1.")
      while retval do
        retval = proxy.del(path.."@1.")
      end
      for _,value in ipairs(selected) do
        local key,add_errmsg = proxy.add(path)
        if not key then
          for _,err in ipairs(add_errmsg) do
            message_helper.pushMessage(T(format("Failed to add %s to %s (%s)",value,name,err.errmsg,err.errcode)),"error")
          end
        else
          local result,set_errmsg = proxy.set(path.."@"..key..".value",value)
          if result then
            apply_changes = true
          else
            for _,err in ipairs(set_errmsg) do
              message_helper.pushMessage(T(format("Failed to add %s to '%s': %s (%s)",value,path,err.errmsg,err.errcode)),"error")
            end
          end
        end
      end
      apply_changes = true
    end
  end

  if apply_changes then
    proxy.apply()
  end
end

local sources_data = content_helper.convertResultToObject("rpc.adblock.source.",proxy.get("rpc.adblock.source."),"paramindex")
local enabled_source = {}
for _,source in ipairs(content_helper.convertResultToObject("uci.adblock.global.adb_sources.", proxy.get("uci.adblock.global.adb_sources."))) do
  enabled_source[untaint(source.value)] = true
end

local warn_box = {
  alert = {
    class = "alert alert-warn",
    style = "text-align:center",
  }
}

ngx.print(ui_helper.createHeader("Adblock",false,true),'\
<div class="modal-body update">')
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-adblock.lp")
  ngx.print('\
  <form class="form-horizontal" method="post" action="modals/adblck-sources-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset><legend>',T"Black List Sources",'</legend>')
    if true then
      ngx.print(ui_helper.createAlertBlock(T"Do not enable too many lists,or you could run out or memory!",warn_box))
    end ngx.print('\
      <style>.unbreakable{display:inline-block;}</style>\
      <div class="control-group">\
        <table id="adb_sources" class="table table-striped">\
          <thead>\
            <tr>\
              <th></th>\
              <th>Name</th>\
              <th>URL</th>\
              <th>Size</th>\
              <th>Focus</th>\
              <th>Description URL</th>\
            </tr>\
          </thead>\
          <tbody>')
          for _,source in ipairs(sources_data) do
           local name = sub(source.paramindex,2)
            local checked = enabled_source[name] and " checked" or ""
            local categories = category[name] ~= nil
            ngx.print('<tr>',
              '<td',categories and ' rowspan="2"' or '','><input type="checkbox" name="adb_sources" value="',name,'"',checked,'></td>',
              '<td',categories and ' rowspan="2"' or '','>',name,'</td>',
              '<td style="whitespace:nowrap">',source.url,'</td>',
              '<td>',source.size,'</td>',
              '<td>',source.focus,'</td>',
              '<td style="whitespace:nowrap"',categories and ' rowspan="2"' or '','><a target="_blank" href="',source.descurl,'">',source.descurl,'&nbsp;<i class="icon-external-link"></i></a></td>',
              '</tr>'
            )
            if categories then
              ngx.print('<tr><td colspan="3">')
                local sources = "adb_"..category[name].."_sources"
                local rpc_path = "rpc.adblock.cat_"..name.."."
                local uci_path = "uci.adblock.global."..sources.."."
                local cat_data = content_helper.convertResultToObject(rpc_path,proxy.get(rpc_path),"category")
                local enabled_cat = {}
                for _,cat in ipairs(content_helper.convertResultToObject(uci_path, proxy.get(uci_path))) do
                  enabled_cat[untaint(cat.value)] = "checked"
                end
                for _,cat in ipairs(cat_data) do
                  local value = untaint((cat.file and cat.file ~= "") and cat.file or cat.category)
                  ngx.print(format("<span class='unbreakable'><input type='checkbox' id='cat_%s' name='%s' value='%s' %s>&nbsp;",category[name],sources,value,enabled_cat[value] or ""))
                  ngx.print(format("<label for='cat_%s'>%s</label>&nbsp;</span>",category[name],cat.category))
                end
              ngx.print('</td></tr>')
            end
          end
          ngx.print('\
          </tbody>\
        </table>\
      </div>\
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
  $(document).off("change",\'input[type\\x3d"checkbox"]\');\
  $(document).on("change",\'input[type\\x3d"checkbox"]\', function(){\
    $("#modal-no-change").addClass("hide");\
    $("#modal-changes").removeClass("hide");\
  });\
</script>\
')

