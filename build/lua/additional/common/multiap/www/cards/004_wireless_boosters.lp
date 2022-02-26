--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local content = {
  agent_enabled = "uci.multiap.agent.enabled",
  controller_enabled = "uci.multiap.controller.enabled",
}
content_helper.getExactContent(content)

if content.agent_enabled and content.agent_enabled ~= "" and content.controller_enabled and content.controller_enabled ~= ""  then
  local ui_helper = require("web.ui_helper")
  local format = string.format

  local agentStatus,controllerStatus
  local boosters = 0
  if content.agent_enabled == "1" then
    agentStatus = "EasyMesh Agent enabled"
  else
    agentStatus = "EasyMesh Agent disabled"
  end
  if content.controller_enabled == "1" then
    controllerStatus = "EasyMesh Controller enabled"
  else
    controllerStatus = "EasyMesh Controller disabled"
  end

  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
      '); ngx.print( ui_helper.createCardHeader(T"Wi-Fi Boosters","/modals/wireless-boosters-modal.lp") ); ngx.print('\
    <div id="booster-card-content" class="content card_bg" data-bg-text="&#xf0e8;">\
      <div>\
      ')
        local html = {}
        html[#html+1] = ui_helper.createSimpleLight(content.controller_enabled,controllerStatus)
        if content.controller_enabled == "1" then
          local modalLink = format('class="modal-link" data-toggle="modal" data-remote="%s" data-id="booster-modal"',"/modals/wireless-boosters-modal.lp")
          html[#html+1] = '<p class="subinfos">'
          html[#html+1] = format(N("<strong %1$s>%2$d booster</strong> found","<strong %1$s>%2$d boosters</strong> found",boosters),modalLink,boosters)
          html[#html+1] = '</p>'
        end
        html[#html+1] = ui_helper.createSimpleLight(content.agent_enabled,agentStatus)
        ngx.print(html)
        ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
<script src="/js/004_wireless_boosters.js"></script>\
');
end
