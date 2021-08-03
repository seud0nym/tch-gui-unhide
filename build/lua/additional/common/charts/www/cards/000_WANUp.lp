--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local attributes = {
  header = {
    class = "wanup_data"
  }
}

ngx.print('\
<div class="span3">\
  <div class="smallcard chartcard">\
    <div class="chartdata wanup_data"><span id="wanup_data">0.00</span><span class="unit">Mb/s</span></div>\
    '); ngx.print(ui_helper.createCardHeader(T"WAN Upload", nil, nil, nil, attributes)); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf0ee;">\
      <canvas id="wanup_canvas"></canvas>\
    </div>\
  </div>\
</div>\
<script>\
var wanup_context = document.getElementById("wanup_canvas").getContext("2d");\
var wanup_config = {\
  type: "line",\
  data: {\
    labels: [60,58,56,54,52,50,48,46,44,42,40,38,36,34,32,30,28,26,24,22,20,18,16,14,12,10,8,6,4,2],\
    datasets: [{\
      borderWidth: 1,\
      borderColor: "rgb(255,194,15)",\
      backgroundColor: "rgb(255,194,15,0.25)",\
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
var wanup_data = sessionStorage.getItem("wanup_data");\
if (wanup_data != null) {\
  var arr = JSON.parse(wanup_data);\
  for (let i = 0; i < arr.length; i++) {\
    wanup_config.data.datasets[0].data[i] = Number(arr[i]);\
  }\
}\
window.wanup_chart = new Chart(wanup_context, wanup_config);\
</script>\
');
