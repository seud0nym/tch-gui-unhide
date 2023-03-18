--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local cards = {
  gateway = "uci.web.card.@card_gateway.hide",
}
content_helper.getExactContent(cards)

if cards.gateway == "0"then
  local ui_helper = require("web.ui_helper")
  local js_chart_rgb_helper = require("js_chart_rgb_helper")
  local rgb = js_chart_rgb_helper.getRGB("cpu")
  local attributes = {
    header = {
      style = "color:rgb("..rgb..");"
    }
  }

  ngx.print('\
  <div class="span3">\
    <div class="smallcard chartcard">\
      <div class="chartdata" style="color:rgb('); ngx.print(rgb); ngx.print(')"><span id="cpu_data">0</span><span class="unit">%</span></div>\
      '); ngx.print(ui_helper.createCardHeader(T"CPU",nil,nil,nil,attributes)); ngx.print('\
      <div class="content card_bg" data-bg-text="&#xf0e4;">\
        <canvas id="cpu_canvas"></canvas>\
      </div>\
    </div>\
  </div>\
  <script>$().ready(function(){$.getScript("/ajax/js_chart_cpu.lua?rgb='); ngx.print(rgb); ngx.print('");});</script>\
  ');
  end
