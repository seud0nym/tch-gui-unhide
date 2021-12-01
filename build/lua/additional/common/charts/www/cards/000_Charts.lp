--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local cards = {
  gateway = "uci.web.card.@card_gateway.hide",
  broadband = "uci.web.card.@card_broadband.hide",
}
content_helper.getExactContent(cards)

if cards.gateway == "0" or cards.broadband == "0" then
  local js_chart_rgb_helper = require("js_chart_rgb_helper")

  local lbl_height = 44
  local cnv_height = 40
  local val_margin = 9
  if cards.gateway == "1" or cards.broadband == "1" then
    lbl_height = 88
    cnv_height = 84
    val_margin = 52
  end

  local charts = {
    { label = "CPU",id = "cpu",unit="%",hide = cards.gateway,},
    { label = "RAM",id = "ram",unit="%",hide = cards.gateway,},
    { label = "WAN &#8595;",id = "wandn",unit="Mb/s",hide = cards.broadband,},
    { label = "WAN &#8593;",id = "wanup",unit="Mb/s",hide = cards.broadband,},
  }

  ngx.print('\
  <style>\
  .chart-lbl{height:'); ngx.print(lbl_height); ngx.print('px;}\
  .chart-cnv{height:'); ngx.print(cnv_height); ngx.print('px;}\
  .chart-val{margin-top:'); ngx.print(val_margin); ngx.print('px;}\
  </style>\
  <div class="span3">\
    <div class="smallcard chartcard chartcard-summary card_bg" data-bg-text="&#xf080;">\
    ')
    local top = 10
    for _,chart in pairs(charts) do
      if chart.hide == "0" then
        chart.rgb = js_chart_rgb_helper.getRGB(chart.id)
        ngx.print('\
        <div style="color:rgb('); ngx.print(chart.rgb); ngx.print(');top:'); ngx.print(top); ngx.print('px;">\
          <div class="chart-lbl">'); ngx.print(chart.label); ngx.print('\
            <div class="chart-val">\
              <span id="'); ngx.print(chart.id); ngx.print('_data">0</span><span class="unit">'); ngx.print(chart.unit); ngx.print('</span>\
            </div>\
          </div>\
          <div class="chart-cnv">\
            <canvas id="'); ngx.print(chart.id); ngx.print('_canvas"></canvas>\
          </div>\
        </div>\
        <script>$().ready(function(){$.getScript("/ajax/js_chart_'); ngx.print(chart.id); ngx.print('.lua?rgb='); ngx.print(chart.rgb); ngx.print('");});</script>\
        ')
        top = top + lbl_height
      end
    end
    ngx.print('\
    </div>\
  </div>\
  ');
end