--pretranslated: do not change this file

ngx.header.content_type = 'application/javascript';
local rgb = ngx.req.get_uri_args()["rgb"]

ngx.print('\
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
    maintainAspectRatio: false,\
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
');
