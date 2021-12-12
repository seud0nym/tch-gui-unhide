--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")

local content = {
  cpuspeed = "uci.power.cpu.cpuspeed",
  cpuwait = "uci.power.cpu.wait",
  ethapd = "uci.power.ethernet.ethapd",
  eee = "uci.power.ethernet.eee",
  autogreeen = "uci.power.ethernet.autogreeen",
  dgm = "uci.power.ethernet.dgm",
}

content_helper.getExactContent(content)

if content.cpuspeed == "256" then
  content.cpuspeed_status = "1"
  content.cpuspeed = T"CPU Frequency Auto-Scaling"
else
  content.cpuspeed_status = "0"
  if content.cpuspeed == "0" then
    content.cpuspeed = T"CPU Frequency 100% (SYNC)"
  elseif content.cpuspeed == "1" then
    content.cpuspeed = T"CPU Frequency 100% (ASYNC)"
  elseif content.cpuspeed == "2" then
    content.cpuspeed = T"CPU Frequency 50% (ASYNC)"
  elseif content.cpuspeed == "4" then
    content.cpuspeed = T"CPU Frequency 25% (ASYNC)"
  elseif content.cpuspeed == "8" then
    content.cpuspeed = T"CPU Frequency 10% (ASYNC)"
  end
end

if content.cpuwait == "1" then
  content.cpuwait_text = T"CPU Wait enabled"
else
  content.cpuwait_text = T"CPU Wait disabled"
end

if content.ethapd == "1" then
  content.ethapd_text = T"Ethernet Auto Power Down enabled"
else
  content.ethapd_text = T"Ethernet Auto Power Down disabled"
end

if content.eee == "1" then
  content.eee_text = T"Energy Efficent Ethernet enabled"
else
  content.eee_text = T"Energy Efficent Ethernet disabled"
end

if content.autogreeen == "1" then
  content.autogreeen_text = T"Ethernet AutoGrEEEn enabled"
else
  content.autogreeen_text = T"Ethernet AutoGrEEEn disabled"
end

if content.dgm == "1" then
  content.dgm_text = T"Ethernet Deep Green Mode enabled"
else
  content.dgm_text = T"Ethernet Deep Green Mode disabled"
end

local modalPath = nil
local session = ngx.ctx.session
if session:hasAccess("/modals/eco-modal.lp") then
  modalPath = "/modals/eco-modal.lp"
end

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeader(T"Eco Settings",modalPath,nil,nil) ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf06c;">\
      <div class="divtable">\
        ');
        local html = {}
        html[#html + 1] = ui_helper.createSimpleLight(content.cpuspeed_status,content.cpuspeed)
        html[#html + 1] = ui_helper.createSimpleLight(content.cpuwait,content.cpuwait_text)
        html[#html + 1] = ui_helper.createSimpleLight(content.ethapd,content.ethapd_text)
        html[#html + 1] = ui_helper.createSimpleLight(content.eee,content.eee_text)
        html[#html + 1] = ui_helper.createSimpleLight(content.autogreeen,content.autogreeen_text)
        html[#html + 1] = ui_helper.createSimpleLight(content.dgm,content.dgm_text)
        ngx.print(html)
        ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
');
