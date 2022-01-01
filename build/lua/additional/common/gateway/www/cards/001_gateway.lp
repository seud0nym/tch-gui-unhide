--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
if session:hasAccess("/modals/gateway-modal.lp") then
  local ui_helper = require("web.ui_helper")
  local headerAttr = {
    icon = {
      class = "icon-info-sign"
    }
  }
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    '); ngx.print(ui_helper.createCardHeader(T"Gateway","modals/gateway-modal.lp",nil,nil,headerAttr)); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf129;">\
      <span class="simple-desc"><i class="icon-time status-icon"></i> System Time: <strong id="gateway-card-time">'); ngx.print(os.date("%d/%m/%Y %H:%M:%S",os.time())); ngx.print('</strong></span>\
      <span class="simple-desc"><i class="icon-circle-arrow-up status-icon"></i> Up: <strong id="gateway-card-uptime">0 hr 0 min 0 sec</strong></span>\
      <span class="simple-desc"><i class="icon-dashboard status-icon"></i> CPU Usage: <strong><span id="gateway-card-cpu" style="width:1.5em;display:inline-block;text-align:right;">0</span>% [<span id="gateway-card-load">0.00 0.00 0.00</span>]</strong></span>\
      <span class="simple-desc"><i class="icon-beaker status-icon"></i> RAM: <span id="gateway-card-ram-total">000</span>MB <strong>[<span id="gateway-card-ram-free">00</span>MB Free]</strong></span>\
      <span class="simple-desc"><i class="icon-hdd status-icon"></i> File System: <span id="gateway-card-disk-total">000.0M</span> <strong>[<span id="gateway-card-disk-free">000.0M</span> Free]</strong></span>\
      <span class="simple-desc"><i class="icon-fire status-icon"></i> Sensor Temperatures: <strong><span id="gateway-card-temps">00° 00°</span></strong></span>\
    </div>\
  </div>\
</div>\
<script src="/js/001_gateway.js"></script>\
');
end
