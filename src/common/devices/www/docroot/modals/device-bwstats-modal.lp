--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local ngx = ngx
local join = table.concat

local vB = post_helper.validateBoolean

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local getargs = ngx.req.get_uri_args()
if getargs.zero and getargs.zero == "1" then
  proxy.set("rpc.gui.bwstats.zero_counters","1")
end

local cfgParams = {
  enabled = "rpc.gui.bwstats.enabled",
}
local cfgValid = {
  enabled = vB,
  show_in_device_list = vB,
}

local config,config_helpmsg = post_helper.handleQuery(cfgParams,cfgValid)
local isEnabled = (config["enabled"] == "1")

local button_attr = { button = { id = "btn-zero" }, group = { class ="monitor-enabled monitor-1", } }
local enabled_attr = { input = { class = "monitor-changes" } }
local alert_attr = { alert = { class = "alert-danger" } }

ngx.print(ui_helper.createHeader(T"Devices",false,false),'\
<div class="modal-body update">\
'); lp.include("tabs-devices.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/device-bwstats-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),
    ui_helper.createAlertBlock(T("Bandwidth usage is <b>indicative</b> <i>only</i>! The counters are not reliable (probably a firmware kernel issue that would require a fix by Technicolor)."),alert_attr))
    if isEnabled then
      local js_chart_rgb_helper = require("js_chart_rgb_helper")
      local rgb = {
        download = js_chart_rgb_helper.getRGB("wandn",true),
        upload = js_chart_rgb_helper.getRGB("wanup",true),
      }
      local labels,download,upload = require("devicesbandwidth_helper").getBandwithStatistics()
      for i=1,#labels do
        labels[i] = '"'..labels[i]..'"'
      end
      ngx.print('\
      <div class="monitor-enabled monitor-1 monitor-hidden-enabled">\
        <canvas id="bwstats_canvas"></canvas>\
      </div>\
      <script>\
        var bwstats_context = document.getElementById("bwstats_canvas").getContext("2d");\
        var bwstats_config = {\
          type: "horizontalBar",\
          options: {\
            responsive: true,\
            plugins: {\
              title: {\
                display: false,\
              }\
            }\
          },\
          data: {\
            labels: [',join(labels,","),'],\
            datasets: [\
              {\
                borderColor: "rgb(',rgb["download"],')",\
                borderWidth: 1,\
                backgroundColor: "rgb(',rgb["download"],',0.25)",\
                data: [',join(download,","),'],\
                label: "Download (MB)",\
              },\
              {\
                borderColor: "rgb(',rgb["upload"],')",\
                borderWidth: 1,\
                backgroundColor: "rgb(',rgb["upload"],',0.25)",\
                data: [',join(upload,","),'],\
                label: "Upload (MB)",\
              }\
            ]\
          },\
        };\
        var bwstats_chart = new Chart(bwstats_context,bwstats_config);\
        bwstats_chart.update();\
        var bwstats_interval_id;\
        function updateDeviceBandwidthStats(){\
          $.post("/ajax/devices-bandwidth.lua",[tch.elementCSRFtoken()],function(data){\
            bwstats_config.data.labels = data.labels;\
            bwstats_config.data.datasets[0].data = data.download;\
            bwstats_config.data.datasets[1].data = data.upload;\
            bwstats_chart.update();\
          },"json")\
          .fail(function(response){\
            if(response.status==403||response.status==404){clearInterval(bwstats_interval_id);}\
          });\
        }\
        bwstats_interval_id = setInterval(updateDeviceBandwidthStats,10000);\
        addRegisteredInterval(bwstats_interval_id);\
      </script>')
    end
    ngx.print('\
    <fieldset>',
    ui_helper.createSwitch(T"Enabled","enabled",config["enabled"],enabled_attr,config_helpmsg["enabled"]),
    ui_helper.createButton("Zero Counters","Reset","icon-refresh",button_attr),'\
      <script>$("#btn-zero").click(function(){tch.loadModal($(".modal form").attr("action")+"?zero=1");})</script>\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter())
