--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")

local find, sub = string.find, string.sub
local floor, ipairs = math.floor, ipairs
local format = string.format

local temp1_input = {}
local temp_output = {}
for i,f in ipairs(temp1_input) do
  temp_output[i] = content_helper.readfile(f,"number",floor) / 1000
end

-- Non-configurable data
local content = {
  ram_total = "sys.mem.RAMTotal",
  ram_free = "sys.mem.RAMFree",
  cpu = "sys.proc.CPUUsage",
}

content_helper.getExactContent(content)

content["uptime"] = content_helper.readfile("/proc/uptime","number",floor)
content["load"] = content_helper.readfile("/proc/loadavg","string"):sub(1,14)
content["temps"] = table.concat(temp_output, "&deg; ") .. "&deg;"

-- Construct an uptime string from the number of seconds
local function secondsToTime(uptime)
  local days =  floor(uptime / 86400)
  local hours =  floor(uptime / 3600) % 24
  local minutes = floor(uptime / 60) % 60
  local seconds = floor(uptime % 60)
  if days > 0 then
    local dayUnit
    if days == 1 then
      dayUnit = "day"
    else
      dayUnit = "days"
    end
    return format(T"%d %s %d hr %d min %d sec", days, dayUnit, hours, minutes, seconds)
  elseif hours > 0 then
    return format(T"%d hr %d min %d sec", hours, minutes, seconds)
  elseif minutes > 0 then
    return format(T"%d min %d sec", minutes, seconds)
  else
    return format(T"%d sec", seconds)
  end
end

local session = ngx.ctx.session
if session:hasAccess("/modals/gateway-modal.lp") then

local headerAttr = {
    icon = {
        class = "icon-info-sign"
    }
}

  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeader(T"Gateway", "modals/gateway-modal.lp", nil, nil, headerAttr) ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf129;">\
      <span class="simple-desc"><i class="icon-time status-icon"></i>&nbsp; System Time: <strong id="gateway-card-time">'); ngx.print(os.date("%d/%m/%Y %H:%M:%S",os.time())); ngx.print('</strong></span>\
      <span class="simple-desc"><i class="icon-circle-arrow-up status-icon"></i>&nbsp; Up: <strong id="gateway-card-uptime">'); ngx.print(secondsToTime(content.uptime)); ngx.print('</strong></span>\
      <span class="simple-desc"><i class="icon-dashboard status-icon"></i>&nbsp; CPU Usage: <strong><span id="gateway-card-cpu">'); ngx.print(content.cpu); ngx.print('</span>% [<span id="gateway-card-load">'); ngx.print(content.load); ngx.print('</span>]</strong></span>\
      <span class="simple-desc"><i class="icon-hdd status-icon"></i>&nbsp; RAM: <span id="gateway-card-ram-total">'); ngx.print(format('%.2f', content.ram_total / 1024)); ngx.print('</span>MB <strong>[<span id="gateway-card-ram-free">'); ngx.print(format('%.2f', content.ram_free / 1024)); ngx.print('</span>MB Free]</strong></span>\
      <span class="simple-desc"><i class="icon-fire status-icon"></i>&nbsp; Sensor Temperatures: <strong><span id="gateway-card-temps">'); ngx.print(content.temps); ngx.print('</span></strong></span>\
    </div>\
  </div>\
</div>\
<script>\
var funcID;\
function secondsToTime(uptime) {\
  var d = Math.floor(uptime / 86400);\
  var h = Math.floor(uptime / 3600) % 24;\
  var m = Math.floor(uptime / 60) % 60;\
  var s = Math.floor(uptime % 60);\
  var dDisplay = d > 0 ? d + " day" + (d == 1 ? " " : "s ") : "";\
  var hDisplay = h > 0 ? h + " hr " : "";\
  var mDisplay = m > 0 ? m + " min " : "";\
  return dDisplay + hDisplay + mDisplay + s + " sec";\
}\
function updateGatewayCard(){\
  $.post("/ajax/gateway-status.lua", [tch.elementCSRFtoken()], function(data){\
    $("#gateway-card-time").text(data["time"]);\
    $("#gateway-card-uptime").text(secondsToTime(Number(data["uptime"])));\
    $("#gateway-card-cpu").text(data["cpu"]);\
    $("#gateway-card-load").text(data["load"]);\
    $("#gateway-card-ram-free").text((Number(data["ram_free"]) / 1024).toFixed(2));\
    $("#gateway-card-ram-total").text((Number(data["ram_total"]) / 1024).toFixed(2));\
    $("#gateway-card-temps").html(data["temps"]);\
  }, "json")\
  .fail(function(response){\
    if(response.status == 403){clearInterval(funcID);window.location.href="/gateway.lp?r="+Date.now();}\
  });\
}\
$().ready(function(){funcID=setInterval(updateGatewayCard,1000);});\
var modalLoaded=false;\
$(document).bind("DOMSubtreeModified",function(){\
  if(!modalLoaded){\
    if($(".modal").length>0){\
      modalLoaded=true;\
      clearInterval(funcID);\
    }\
  }\
});\
</script>\
');  
end
  ngx.print('\
'); 