--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local find,gsub,match = string.find,string.gsub,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

if ngx.var.request_method == "POST" then
  local args = ngx.req.get_post_args()
  if args.action == "SAVE" then
    local action = args.TGU_update_action
    if action == "ignore" then
      proxy.set("rpc.gui.IgnoreCurrentRelease","1")
    elseif action == "now" then
      local result,errors = proxy.set("rpc.gui.UpdateNow",untaint(proxy.get("uci.version.version.@version[0].marketing_version")[1].value))
      if not result then
        for _,err in ipairs(errors) do
          message_helper.pushMessage(T(err.errmsg),"error")
        end
      end
    end
  end
end

local current = {
  available = "rpc.gui.UpdateAvailable",
  version = "rpc.gui.UnhideVersion",
  ignored = "rpc.gui.IgnoreCurrentRelease",
}
content_helper.getExactContent(current)

local changelog = {}
local curl = io.popen("curl -qskL https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/CHANGELOG.txt",'r')
if curl then
  for line in curl:lines() do
    local release = match(line,"^(%d%d%d%d%.%d%d%.%d%d)")
    if release and release <= current.version then
      break
    end
    if release or line == "" or find(line,"^=====") or find(line,"^%s*$") or find(line,"^%* tch%-gui%-unhide") then
      changelog[#changelog+1] = gsub(line,"^%* tch%-gui%-unhide: ","- ")
    end
  end
  curl:close()
end

local update_options = {
  {"nothing",T""},
  {"manual",T"Manually Upgrade tch-gui-unhide"},
  {"now",T"Upgrade tch-gui-unhide Now"},
  {"ignore",T"Ignore This Update"},
}

ngx.print(ui_helper.createHeader(T"Update Available",false,false)); ngx.print('\
<div class="modal-body update">\
  <form id="TGU_update_form" class="form-horizontal" method="post" action="modals/tch-gui-unhide-update-modal.lp">\
    ');  
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    if current.available == "1" and current.ignored == "0" then
      local monitor_attr = { select = { class = "monitor-changes" },}
      local nothing_attr = { alert = { class = "alert-info monitor-TGU_update_action monitor-nothing" },} 
      local manual_attr = { alert = { class = "alert-info monitor-TGU_update_action monitor-manual" },} 
      local now_attr = { alert = { class = "alert-info monitor-TGU_update_action monitor-now" },}
      local ignore_attr = { alert = { class = "alert-info monitor-TGU_update_action monitor-ignore" },}
      ngx.print('\
      <fieldset>\
        ');  
        ngx.print(ui_helper.createInputSelect(T"Choose an Action:","TGU_update_action",update_options,"nothing",monitor_attr))
        ngx.print(ui_helper.createAlertBlock(T("Choose an action."),nothing_attr))
        ngx.print(ui_helper.createAlertBlock(T("A new browser window will open for you to manually download the latest release and install it yourself."),manual_attr))
        ngx.print(ui_helper.createAlertBlock(T("The upgrade will be automatically downloaded and applied. The latest utility scripts will be extracted to the /tmp directory."),now_attr))
        ngx.print(ui_helper.createAlertBlock(T("The 'Update Available' button will be hidden until the next release."),ignore_attr))
        ngx.print('\
      </fieldset>\
      ');
    end
    if #changelog == 0 then
      ngx.print("Up to date!")
    else
      ngx.print('\
      <fieldset>\
        <legend>'); ngx.print( T"Changelog" ); ngx.print('</legend>\
        <pre>');  
        ngx.print(table.concat(changelog,"\n"))
        ngx.print('\
        </pre>\
      </fieldset>\
      ');
    end
    ngx.print('\
  </form>\
</div>\
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
<div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>\
<script>\
  $("#save-config").on("click",function(){\
    var action=$("#TGU_update_action").val();\
    if (action=="manual") {\
      window.open("https://github.com/seud0nym/tch-gui-unhide/releases/latest","_blank");\
      $("#tch-gui-unhide-update-modal > div.modal-header > div > div.span1 > a").trigger("click");\
      return false;\
    } else {\
      if (action!="nothing") {\
        if (confirm("Are you sure?")) {\
          var busy_msg = $(".loading-wrapper");\
          busy_msg.removeClass("hide");\
          busy_msg[0].scrollIntoView();\
          $(".modal-body").scrollLeft(0);\
          if (action=="now"){\
            setTimeout(function(){alert("Upgrade in progress...\\n\\n(Messages are being sent to the system log.)\\n\\nPlease wait a minute or two and then refresh manually if it does not happen automatically.");},1000);\
            setTimeout(function(){window.location.href="/gateway.lp?r="+Date.now();},120000);\
          }\
          return true;\
        }\
        return false;\
      }\
      return true;\
    }\
  });\
</script>\
'); 
