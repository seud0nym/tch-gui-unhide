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
    rgb = "0,188,212"
  end
else
  if color == "MONOCHROME" then
    rgb = "128,128,128"
  else
    rgb = "0,0,255"
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
    <div class="chartdata" style="color:rgb('); ngx.print(rgb); ngx.print(')"><span id="cpu_data">0</span><span class="unit">%</span></div>\
    '); ngx.print(ui_helper.createCardHeader(T"CPU", nil, nil, nil, attributes)); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf0e4;">\
      <canvas id="cpu_canvas"></canvas>\
    </div>\
  </div>\
</div>\
<script>\
var cpu_context = document.getElementById("cpu_canvas").getContext("2d");\
var cpu_config = {\
  type: "line",\
  data: {\
    labels: [60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1],\
    datasets: [{\
      borderWidth: 1,\
      borderColor: "rgb('); ngx.print(rgb); ngx.print(')",\
      backgroundColor: "rgb('); ngx.print(rgb); ngx.print(',0.25)",\
      data: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],\
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
          max: 100,\
          min: 0,\
          stepSize: 10\
        }\
      }]\
    }\
  }\
};\
var cpu_data = sessionStorage.getItem("cpu_data");\
if (cpu_data != null) {\
  var arr = JSON.parse(cpu_data);\
  for (let i = 0; i < arr.length; i++) {\
    cpu_config.data.datasets[0].data[i] = Number(arr[i]);\
  }\
}\
window.cpu_chart = new Chart(cpu_context, cpu_config);\
</script>\
');
