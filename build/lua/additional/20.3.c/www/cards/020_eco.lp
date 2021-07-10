--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")

local content = {
  apd = "uci.power.ethernet.apd",
  dgm = "uci.power.ethernet.dgm",
  eee = "uci.power.ethernet.eee",
  net = "uci.power.ethernet.net",
  phy = "uci.power.ethernet.phy",
}

content_helper.getExactContent(content)

if content.apd == "1" then
  content.apd_text = T"Ethernet Auto Power Down enabled"
else
  content.apd_text = T"Ethernet Auto Power Down disabled"
end

if content.dgm == "1" then
  content.dgm_text = T"Ethernet Deep Green Mode enabled"
else
  content.dgm_text = T"Ethernet Deep Green Mode disabled"
end

if content.eee == "1" then
  content.eee_text = T"Energy Efficent Ethernet enabled"
else
  content.eee_text = T"Energy Efficent Ethernet disabled"
end

if content.net == "1" then
  content.net_text = T"Network Device Down enabled"
else
  content.net_text = T"Network Device Down disabled"
end

if content.phy == "1" then
  content.phy_text = T"PHY Power Down enabled"
else
  content.phy_text = T"PHY Power Down disabled"
end

local modalPath = nil
local session = ngx.ctx.session
if session:hasAccess("/modals/eco-modal.lp") then
  modalPath = "/modals/eco-modal.lp"
end

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeader(T"Eco Settings", modalPath, nil, nil) ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf06c;">\
      <div class="divtable">\
        ');
        local html = {}
        html[#html + 1] = ui_helper.createSimpleLight(content.apd, content.apd_text)
        html[#html + 1] = ui_helper.createSimpleLight(content.dgm, content.dgm_text)
        html[#html + 1] = ui_helper.createSimpleLight(content.eee, content.eee_text)
        html[#html + 1] = ui_helper.createSimpleLight(content.net, content.net_text)
        html[#html + 1] = ui_helper.createSimpleLight(content.phy, content.phy_text)
        ngx.print(html)
        ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
'); 
