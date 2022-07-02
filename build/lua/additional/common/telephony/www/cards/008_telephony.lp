--pretranslated: do not change this file

local lfs = require("lfs")
if lfs.attributes("/etc/config/mmpbx","mode") ~= "file" then
  return
end

-- Enable localization
gettext.textdomain('webui-voice')

local proxy = require ("datamodel")
local ui_helper = require ("web.ui_helper")
local content_helper = require ("web.content_helper")
local tch = require("telephonycard_helper")

local format = string.format

local session = ngx.ctx.session
if session:hasAccess("/modals/mmpbx-info-modal.lp") then
  local mmpbx_state = proxy.get("uci.mmpbx.mmpbx.@global.enabled")
  if mmpbx_state then
    mmpbx_state = mmpbx_state[1].value
  else
    mmpbx_state = "0"
  end

  local modalPath
  if mmpbx_state == "0" and session:hasAccess("/modals/mmpbx-global-modal.lp") then
    modalPath = "/modals/mmpbx-global-modal.lp"
  else
    modalPath = "/modals/mmpbx-info-modal.lp"
  end

  local dect = {
    state = "rpc.mmpbx.dectemission.state",
  }
  content_helper.getExactContent(dect)

  local emission_info
  if dect.state then
    if dect.state == "1" then
      emission_info = T"DECT Emission Mode enabled"
    elseif dect.state == "0" then
      emission_info = T"DECT Emission Mode disabled"
    end
  end

  local html,disabled = tch.getTelephonyCardHTML(mmpbx_state)
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    '); ngx.print(ui_helper.createCardHeader(T"Telephony",modalPath)); ngx.print('\
    <div class="content">\
      <div id="telephony-card-content">\
      '); ngx.print(html); ngx.print('\
      </div>\
      ');
        if disabled ~= 0 then
          ngx.print('<span class="modal-link" data-toggle="modal" data-remote="/modals/mmpbx-profile-modal.lp" data-id="mmpbx-profile-modal">')
          ngx.print(ui_helper.createSimpleLight("0",format(N('%d disabled profile','%d disabled profiles',disabled),disabled)))
          ngx.print('</span>')
        end
        if emission_info then
          ngx.print('<span class="modal-link" data-toggle="modal" data-remote="/modals/mmpbx-dect-modal.lp" data-id="mmpbx-dect-modal">')
          ngx.print(ui_helper.createSimpleLight(dect.state,emission_info))
          ngx.print('</span>')
        end
        ngx.print('\
    </div>\
  </div>\
</div>\
<script>\
'); if mmpbx_state == "1" then ngx.print('\
  var tFuncID;\
  function updateTelephonyCard(){\
    $.post("/ajax/telephony-status.lua",[tch.elementCSRFtoken()],function(data){\
      $("#telephony-card-content").html(data["html"]);\
    },"json")\
    .fail(function(response){\
      if(response.status==403||response.status==404){clearInterval(tFuncID);}\
    });\
  }\
  $().ready(function(){tFuncID=setInterval(updateTelephonyCard,3000);addRegisteredInterval(tFuncID);});\
'); end; ngx.print('\
</script>\
');
  end

