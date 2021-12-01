--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local format = string.format
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local bbch = require("broadbandcard_helper")

-- wan status data
local wan_data = {
  wans_enable = "uci.wansensing.global.enable",
  wans_l2type = "uci.wansensing.global.l2type",
  wans_l3type = "uci.wansensing.global.l3type",
}
content_helper.getExactContent(wan_data)

local WS_en = wan_data.wans_enable
local WS_L2 = wan_data.wans_l2type
local WS_L3 = wan_data.wans_l3type
if WS_L2 == "" then
  WS_L2 = "NONE"
end
local WS_State
if WS_en == "1" then
  WS_Text = T"WAN Sensing enabled"
  if WS_L2 == "" then
    WS_L2 = "NONE"
    WS_State = "None"
  else
    WS_State = "Enabled"
  end
  if WS_L3 == "" then
    WS_L3 = "NONE"
  end
else
   WS_Text = T"WAN Sensing disabled"
   WS_State = "Disabled"
end

local WS_light_map = {
  Disabled = "off",
  None = "orange",
  Enabled = "green",
}

local session = ngx.ctx.session
local modalPath
if session:hasAccess("/modals/broadband-modal.lp") then
    modalPath = "modals/broadband-modal.lp"
end

local html = bbch.getBroadbandCardHTML(WS_en)

  ngx.print('\
<div class="span3" id="broadband-card">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeader(T"Broadband",modalPath,nil,nil,nil) ); ngx.print('\
    <div id="broadband-card-throughput"><div class="throughput"><span title="Raw throughput including protocol overhead"></span></div></div>\
    <div class="content card_bg mirror" data-bg-text="&#xf0c1;">\
      ');  
      ngx.print( ui_helper.createSimpleLight(nil,WS_Text,{ light = { class = WS_light_map[WS_State] } }) )
      if WS_en == "1" then
        ngx.print( format('<p class="subinfos">L2: <strong>%s</strong> L3: <strong>%s</strong></p>',WS_L2,WS_L3) )
      end
      ngx.print('\
      <div id="broadband-card-content">\
        ');  ngx.print(html); ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
<script>\
var bbFuncID;\
var tpFuncID;\
function updateBroadbandCard(){\
  $.post("/ajax/broadband-status.lua",[tch.elementCSRFtoken()],function(data){\
    $("#broadband-card-content").html(data["html"]);\
  },"json")\
  .fail(function(response){\
    if(response.status==403||response.status==404){clearInterval(bbFuncID);}\
  });\
}\
function updateThroughput(){\
  $.post("/ajax/network-throughput.lua",[tch.elementCSRFtoken()],function(data){\
    $("#broadband-card-throughput .throughput span").html(data["wan"]);\
    $("#lan-card-throughput .throughput span").html(data["lan"]);\
    $("#wanup_data").text(data["wan_tx_mbps"].toFixed(2));\
    if (typeof window.wanup_chart != "undefined") {\
      wanup_config.data.datasets[0].data.shift();\
      wanup_config.data.datasets[0].data.push(data["wan_tx_mbps"]);\
      window.wanup_chart.update();\
      sessionStorage.setItem("wanup_data",JSON.stringify(wanup_config.data.datasets[0].data));\
    }\
    if (typeof window.wandn_chart != "undefined") {\
      $("#wandn_data").text(data["wan_rx_mbps"].toFixed(2));\
      wandn_config.data.datasets[0].data.shift();\
      wandn_config.data.datasets[0].data.push(data["wan_rx_mbps"]);\
      window.wandn_chart.update();\
      sessionStorage.setItem("wandn_data",JSON.stringify(wandn_config.data.datasets[0].data));\
    }\
  },"json")\
  .fail(function(response){\
    if(response.status==403||response.status==404){clearInterval(tpFuncID);}\
  });\
}\
$().ready(function(){\
  bbFuncID=setInterval(updateBroadbandCard,15000);\
  var bbdiv = document.querySelector("#broadband-card-throughput");\
  var bbhdr = document.querySelector("#broadband-card .header-title");\
  bbhdr.parentNode.insertBefore(bbdiv,bbhdr.nextSibling);\
  var landiv = bbdiv.cloneNode(true);\
  var lanhdr = document.querySelector("#Local,#Local_Network_tab").closest(".header-title");\
  landiv.setAttribute("id","lan-card-throughput");\
  lanhdr.parentNode.insertBefore(landiv,lanhdr.nextSibling);\
  updateThroughput();\
  tpFuncID=setInterval(updateThroughput,2000);\
});\
</script>\
'); 