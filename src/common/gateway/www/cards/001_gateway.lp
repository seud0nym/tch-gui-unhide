--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
if session:hasAccess("/modals/gateway-modal.lp") then
  local ui_helper = require("web.ui_helper")
  local gch = require("gatewaycard_helper")
  local format = string.format

  local data = gch.getGatewayCardData()

  local headerAttr = {
    p = {
      id = "Gateway_tab"
    },
    icon = {
      class = "icon-info-sign"
    }
  }
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader("Gateway","modals/gateway-modal.lp",nil,nil,headerAttr),'\
    <div class="content card_bg" data-bg-text="&#xf129;">\
      <span class="simple-desc"><i class="icon-time status-icon"></i>Time: <strong id="gateway-card-time">',T(data.time),'</strong></span>\
      <span class="simple-desc"><i class="icon-circle-arrow-up status-icon"></i>Up: <strong id="gateway-card-uptime">',T(gch.secondsToTime(data.uptime)),'</strong></span>\
      <span class="simple-desc"><i class="icon-dashboard status-icon"></i>CPU: <strong><span id="gateway-card-cpu" style="width:1.5em;display:inline-block;text-align:right;">',T(format("%d",data.cpu)),'</span>% [<span id="gateway-card-load">',T(data.load),'</span>]</strong></span>\
      <span class="simple-desc"><i class="icon-beaker status-icon"></i>RAM: <span id="gateway-card-ram-total">',T(format("%.2f",data.ram_total/1024)),'</span>M <strong id="ram_free">[<span id="gateway-card-ram-free">',T(format("%.2f",data.ram_free/1024)),'</span>M Free]</strong><strong id="ram_avail" class="hide">[<span id="gateway-card-ram-avail">',T(format("%.2f",data.ram_avail/1024)),'</span>M Available]</strong></span>\
      <span class="simple-desc"><i class="icon-hdd status-icon"></i>Disk: <span id="gateway-card-disk-total">',T(data.disk_total),'</span> <strong>[<span id="gateway-card-disk-free">',T(data.disk_free),'</span> Free]</strong></span>\
      <span class="simple-desc"><i class="icon-fire status-icon"></i>Temperatures: <strong><span id="gateway-card-temps">',T(data.temps),'</span></strong></span>\
    </div>\
  </div>\
</div>\
<script>$("#"+"Gateway"+"_tab").append("<span style=\'font-size:x-small\'> =SERIAL=</span>")</script>\
<script src="/js/001_gateway.js"></script>\
');
end
