--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")

local function resetreboot(path,value)
  proxy.set(path,value)
  ngx.header.content_type = "application/json"
  ngx.print('{ "success":"true" }')
  ngx.timer.at(0,function(_)
    proxy.apply()
  end)
  ngx.exit(ngx.HTTP_OK)
end

-- Handle fwupgrade/configimport/configexport specific GET/POSTs.
if ngx.req.get_method() == "POST" then
  local action = ngx.req.get_post_args().action
  if action == "system_bootp" then
    return resetreboot("sys.bootp.enable","1")
  end
  if action == "system_switchover" then
    return resetreboot("rpc.system.switchover","1")
  end
end

local bootp_alert = {
  alert = {
    class = "alert-info hide",
    id = "bootp-msg"
  }
}
local bootp = {
  group = {
    class = "wait-for-reboot"
  },
  button = {
    id = "btn-system-bootp",
    class = "custom-handler"
  }
}
local lose_root_warning = {
  alert = {
    class = "alert-danger",
    style = "margin-bottom:5px;"
  }
}
local switchover_alert = {
  alert = {
    class = "alert-info hide",
    id = "switchover-msg"
  }
}
local switchover = {
  group = {
    class = "wait-for-reboot"
  },
  button = {
    id = "btn-system-switchover",
    class = "custom-handler"
  }
}

ngx.print(ui_helper.createHeader(T"System Extras",false,false),'\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-system-extras.lp")
ngx.print(
  ui_helper.createMessages(message_helper.popMessages()),'\
  <form class="form-horizontal" method="post" action="modals/system-reboot-modal.lp">\
    <fieldset>\
      <legend>',T"Advanced Reboot",'</legend>',
      ui_helper.createButton(T"LAN Update",T"BOOTP","icon-exchange",bootp),
      ui_helper.createAlertBlock(T"Entering BOOTP. Please wait...",bootp_alert),
      ui_helper.createAlertBlock(T"Root access will be <i>lost</i> when using the <b><i class='icon-exchange' style='width:auto;'></i> Switchover</b> button <i>if</i> you have not previously acquired root on the passive bank.", lose_root_warning),
      ui_helper.createButton(T"Switch Active Bank",T"Switchover","icon-exchange",switchover),
      ui_helper.createAlertBlock(T"Switching bank. Please wait...",switchover_alert), -- need to keep the quote on the next line because this line is REMOVED on FW 20.4 (165)
      '\
    </fieldset>\
  </form>\
</div>\
<script>\
(function() {\
  var refreshTimeOut = 5000;\
  var refreshDelay = 3000;\
  var target = $(".modal form").attr("action");\
  function wait_for_webserver_running() {\
    $.ajax({ url: "/",timeout: refreshTimeOut,data: { check: "1"} })\
    .done(function(data) {\
      document.open("text/html");\
      document.write(data);\
      document.close();\
    })\
    .fail(function() {\
      window.setTimeout(wait_for_webserver_running,refreshDelay);\
    });\
  }\
  function wait_for_webserver_down() {\
    $.ajax({ url: target,timeout: refreshTimeOut,data: { check: "1"}  })\
    .done(function() {\
      window.setTimeout(wait_for_webserver_down,refreshDelay);\
    })\
    .fail(function() {\
      window.setTimeout(wait_for_webserver_running,refreshDelay);\
    });\
  }\
  function resetreboot(msg,msg_dst,action) {\
    msg_dst.after(msg);\
    msg.removeClass("hide");\
    msg[0].scrollIntoView();\
    $.post(\
      target,\
      { action: action,CSRFtoken: $("meta[name=CSRFtoken]").attr("content") },\
      wait_for_webserver_down,\
      "json"\
    );\
    return false;\
  }\
  $("#btn-system-bootp").on("click",function() {\
     if (confirm("',T"This will put your gateway into LAN Update",'")){\
       return resetreboot($("#bootp-msg"),$(this),"system_bootp");\
     }\
  });\
  $("#btn-system-switchover").on("click",function() {\
     if (confirm("',T"This will switch the active bank with the passive one",'")){\
       return resetreboot($("#switchover-msg"),$(this),"system_switchover");\
     }\
  });\
}());\
</script>\
',ui_helper.createFooter())
