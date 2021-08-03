--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local attributes = {
  header = {
    class = "ram_data"
  }
}

ngx.print('\
<div class="span3">\
  <div class="smallcard chartcard">\
    <div class="chartdata ram_data"><span id="ram_data">0</span><span class="unit">%</span></div>\
    '); ngx.print(ui_helper.createCardHeader(T"RAM", nil, nil, nil, attributes)); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf0c3;">\
      <canvas id="ram_canvas"></canvas>\
    </div>\
  </div>\
</div>\
<script>\
var ram_context = document.getElementById("ram_canvas").getContext("2d");\
var ram_config = {\
  type: "line",\
  data: {\
    labels: [60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1],\
    datasets: [{\
      borderWidth: 1,\
      borderColor: "rgb(148,41,116)",\
      backgroundColor: "rgb(148,41,116,0.25)",\
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
var ram_data = sessionStorage.getItem("ram_data");\
if (ram_data != null) {\
  var arr = JSON.parse(ram_data);\
  for (let i = 0; i < arr.length; i++) {\
    ram_config.data.datasets[0].data[i] = Number(arr[i]);\
  }\
}\
window.ram_chart = new Chart(ram_context, ram_config);\
</script>\
');
