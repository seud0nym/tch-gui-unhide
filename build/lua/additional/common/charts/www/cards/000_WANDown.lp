--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local TGU_Theme = ngx.shared.TGU_Theme
local theme = TGU_Theme:get("THEME") or ""
local color = TGU_Theme:get("COLOR") or ""

local rgb
if theme == "night" then
  if color == "MONOCHROME" then
    rgb = "192,192,192"
  else
    rgb = "0,255,0"
  end
else
  if color == "MONOCHROME" then
    rgb = "128,128,128"
  else
    rgb = "0,165,64"
  end
end

local attributes = {
  header = {
    style = "color:rgb("..rgb..");"
  }
}

ngx.print('\
<div class="span3">\
  <div class="smallcard chartcard">\
    <div class="chartdata" style="color:rgb('); ngx.print(rgb); ngx.print(')"><span id="wandn_data">0.00</span><span class="unit">Mb/s</span></div>\
    '); ngx.print(ui_helper.createCardHeader(T"WAN Download", nil, nil, nil, attributes)); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf0ed;">\
      <canvas id="wandn_canvas"></canvas>\
    </div>\
  </div>\
</div>\
<script>\
var wandn_context = document.getElementById("wandn_canvas").getContext("2d");\
var wandn_config = {\
  type: "line",\
  data: {\
    labels: [60,58,56,54,52,50,48,46,44,42,40,38,36,34,32,30,28,26,24,22,20,18,16,14,12,10,8,6,4,2],\
    datasets: [{\
      borderWidth: 1,\
      borderColor: "rgb('); ngx.print(rgb); ngx.print(')",\
      backgroundColor: "rgb('); ngx.print(rgb); ngx.print(',0.25)",\
      data: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],\
      fill: true,\
      pointRadius: 0\
    }]\
  },\
  options: {\
    responsive: true,\
    legend: {\
      display: false\
    },\
    scales: {\
      xAxes: [{\
        display: false\
      }],\
      yAxes: [{\
        display: false,\
        ticks: {\
          min: 0\
        }\
      }]\
    }\
  }\
};\
var wandn_data = sessionStorage.getItem("wandn_data");\
if (wandn_data != null) {\
  var arr = JSON.parse(wandn_data);\
  for (let i = 0; i < arr.length; i++) {\
    wandn_config.data.datasets[0].data[i] = Number(arr[i]);\
  }\
}\
window.wandn_chart = new Chart(wandn_context, wandn_config);\
</script>\
');
