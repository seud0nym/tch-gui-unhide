<%
local lfs = require("lfs")
if lfs.attributes("/etc/config/mmpbx", "mode") ~= "file" then
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
  local mmpbx_state, error = proxy.get("uci.mmpbx.mmpbx.@global.enabled")
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

  local emission_content = {
    emission_state = "rpc.mmpbx.dectemission.state",
  }
  content_helper.getExactContent(emission_content)
  local emission_state = format("%s", emission_content.emission_state)

  local emission_info
  if emission_state and emission_state == "1" then
    emission_info = T"DECT Emission Mode enabled"
  else
    emission_info = T"DECT Emission Mode disabled"
  end

  local switchName = "mmpbx_enable"

  local html, disabled = tch.getTelephonyCardHTML(mmpbx_state)
%>
<div class="span3">
  <div class="smallcard">
    <% ngx.print(ui_helper.createCardHeader(T"Telephony", modalPath, switchName, mmpbx_state)) %>
    <div class="content">
      <div id="telephony-card-content">
        <% ngx.print(html) %>
      </div>
      <%
        ngx.print('<span class="modal-link" data-toggle="modal" data-remote="/modals/mmpbx-dect-modal.lp" data-id="mmpbx-dect-modal">')
        ngx.print(ui_helper.createSimpleLight(emission_state, emission_info))
        ngx.print('</span>')
        if disabled ~= 0 then
          ngx.print('<p class="subinfos modal-link" data-toggle="modal" data-remote="/modals/mmpbx-profile-modal.lp" data-id="mmpbx-profile-modal">')
          ngx.print(format(N('<strong>%d</strong> disabled profile','<strong>%d</strong> disabled profiles',disabled),disabled))
          ngx.print('</p>')
        end
      %>
    </div>
  </div>
</div>
<script>
$(".header input#mmpbx_enable").on("change",function(){$("#waiting").addClass("do-not-show").removeClass("hide")});
<% if mmpbx_state == "1" then %>
  var tFuncID;
  function updateTelephonyCard(){
    $.post("/ajax/telephony-status.lua", [tch.elementCSRFtoken()], function(data){
      $("#telephony-card-content").html(data["html"]);
    }, "json")
    .fail(function(response){
      if(response.status==403||response.status==404){clearInterval(tFuncID);}
    });
  }
  $().ready(function(){tFuncID=setInterval(updateTelephonyCard,3000);window.intervalIDs.push(tFuncID);});
<% end %>
</script>
<%
end
%>
