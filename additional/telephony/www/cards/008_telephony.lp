<%
local lfs = require("lfs")
if lfs.attributes("/etc/config/mmpbx", "mode") ~= "file" then
  return
end

-- Enable localization
gettext.textdomain('webui-voice')

datamodel = require ("datamodel")
ui_helper = require ("web.ui_helper")
content_helper = require ("web.content_helper")

local session = ngx.ctx.session
if session:hasAccess("/modals/mmpbx-info-modal.lp") then
  local currentuserrole = session.getrole()

  local sipprofile_path = "uci.mmpbxrvsipnet.profile."
  local sipprofile_ubus_path = "rpc.mmpbx.profile.@"

  local sipprofile_content = content_helper.getMatchedContent (sipprofile_path)

  local mmpbx_state, error = datamodel.get("uci.mmpbx.mmpbx.@global.enabled")
  if mmpbx_state then
    mmpbx_state = mmpbx_state[1].value
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
  local emission_state = string.format("%s", emission_content.emission_state)

  local emission_info
  if emission_state and emission_state == "1" then
    emission_info = T"DECT Emission Mode enabled"
  else
    emission_info = T"DECT Emission Mode disabled"
  end

  local sipprofile_info = {}
  local v = {}
  for _, v in pairs (sipprofile_content) do
    local name = string.match (v.path, "@([^%.]+)")
    if v.userfriendlyname ~= nil and v.userfriendlyname ~= "" then
      sipprofile_info[name] = v.userfriendlyname
    else
      sipprofile_info[name] = v.uri
    end
  end

  local switchName = "mmpbx_enable"
%>

<div class="span3">
  <div class="smallcard">
    <%
      ngx.print(ui_helper.createCardHeader(T"Telephony", modalPath, switchName, mmpbx_state))
    %>
    <div class="content">
    <%
      local name, value = "", ""
      local reg_info = {
        ["0"] = "Unregistered Accounts",
        ["1"] = "Registered Accounts",
        ["2"] = "Registering Accounts",
      }
      local reg_num = {["0"] = 0,["1"] = 0,["2"] = 0}
      local reg_state = {"1","2","0"}
      for name, value in pairs (sipprofile_info) do
        local res = datamodel.get (sipprofile_ubus_path..name..".sipRegisterState")
          if (type(res) == "table") then
            if (res[1].value == "Registered") then
              reg_num["1"] = reg_num["1"] + 1
              ngx.print (ui_helper.createSimpleLight("1", name .. T" registered"))
            elseif (res[1].value == "Registering") then
              reg_num["2"] = reg_num["2"] + 1
              ngx.print (ui_helper.createSimpleLight("2", name .. T" registering"))
            end
          end
          reg_num["0"] = reg_num["0"] + 1
      end
      reg_num["0"] = reg_num["0"] - reg_num["1"] - reg_num["2"]
      if (reg_num["0"] ~= 0) then
          ngx.print('<p class="subinfos">')
          ngx.print(string.format(T'%d %s<br />', reg_num["0"], reg_info["0"]))
          ngx.print('</p>')
      end
      if emission_state == "1" or emission_state == "0" then
        ngx.print (ui_helper.createSimpleLight(emission_state, emission_info))
      end
    %>
    </div>
  </div>
</div>
<script>$(".header input#mmpbx_enable").on("change",function(){$("#waiting").removeClass("hide")});</script>
<%
end
%>
