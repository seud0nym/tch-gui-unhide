--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local untaint_mt = require("web.taint").untaint_mt
local wireless_qr = require("wireless_qr")
local format,match,toupper = string.format,string.match,string.upper
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local getargs = ngx.req.get_uri_args()
local getiface = format("%s",getargs.iface or "wl0")
local getap = format("%s",getargs.ap or "ap0")
local ifacepath = "uci.wireless.wifi-iface.@"..getiface
local appath = "uci.wireless.wifi-ap.@"..getap

if not proxy.get(appath..".security_mode") then
  appath = ifacepath
end

local content = {
  ssid = ifacepath..".ssid",
  device = ifacepath..".device",
  secmode = appath..".security_mode",
  wpakey = appath..".wpa_psk_key",
  public = appath..".public",
}
if ifacepath ~= appath then
  content["wepkey"] = appath..".wep_key"
end
content_helper.getExactContent(content)

local secmodes = setmetatable({
  ["none"] =  "nopass" ,
  ["wpa2-psk"] = "WPA",
  ["wpa-wpa2-psk"] = "WPA",
  ["wpa2-wpa3-psk"] = "WPA",
  ["wpa2"] = "WPA",
  ["wpa-wpa2"] =  "WPA",
  ["wep"] =  "WEP",
},untaint_mt)

local keypassphrases = setmetatable({
  ["none"] = "",
  ["wpa2-psk"] = content.wpakey,
  ["wpa-wpa2-psk"] = content.wpakey,
  ["wpa2-wpa3-psk"] = content.wpakey,
  ["wep"] = content.wepkey,
},untaint_mt)

local wlanconfstr = wireless_qr.format(content,secmodes,keypassphrases,false)
local password,hidden = match(wlanconfstr,"WIFI:T:[^:]+;S:[^:]+;P:([^:]+);H:([^:]+);;")
local ssid = untaint(content.ssid)
local secmode = toupper(untaint(content.secmode))

local monitor_attr = {
  input = {
    class = "monitor-changes"
  }
}

ngx.print('\
'); ngx.print(ui_helper.createHeader(ssid,false,false)); ngx.print('\
<div class="modal-body update">\
  <form class="form-horizontal" method="post" action="modals/wireless-qrcode-modal.lp">\
    <fieldset id="printable">\
      <center id="print-body" style="background-color:white;color:black;">\
        <img src="/img/Wi-Fi-Logo.png" style="height:120px" class="monitor-logo monitor-1">\
        <br>\
        <div class="monitor-ssid monitor-1">\
          <br>\
          WiFi Network:\
          <h2>');
            ngx.print(ssid);
            if hidden == "true" then
              ngx.print('<br><i><small>(Hidden)</small></i>')
            end
            ngx.print('</h2>\
          ')
          ngx.print('\
          <br>\
        </div>\
        <div id="qrcode"></div>\
        <div class="monitor-password monitor-1">\
          <br>\
          Password: <strong>'); ngx.print(password); ngx.print('</strong> Mode: '); ngx.print(secmode); ngx.print('\
        </div>\
        <br>\
      <center>\
    </fieldset>\
    ')
    ngx.print(ui_helper.createSwitch(T"Show Logo","logo","1",monitor_attr))
    ngx.print(ui_helper.createSwitch(T"Show SSID","ssid","1",monitor_attr))
    ngx.print(ui_helper.createSwitch(T"Show Password","password","1",monitor_attr))
    ngx.print('\
  </form>\
  <div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>\
</div>\
<div class="modal-footer">\
  <div id="btn-close" class="btn btn-large">');  ngx.print(T"Back"); ngx.print('</div>\
  <div id="btn-print" class="btn btn-primary btn-large">');  ngx.print(T"Print"); ngx.print('</div>\
</div>\
<script src="/js/qrcode-min.js"></script>\
<script>\
$(function(){\
    let wlanconfstr = $("<textarea/>").html("'); ngx.print(wlanconfstr); ngx.print('").text();\
    let typeNumber = 8;\
    let errorCorrectLevel = \'M\';\
    let qr = qrcode(typeNumber,errorCorrectLevel);\
    qr.addData(wlanconfstr);\
    qr.make();\
    qr.createImgTag(typeNumber);\
    let html = qr.createTableTag(typeNumber);\
    $("#qrcode").html(html);\
    $("#btn-close").click(function() {\
      let busy_msg = $(".loading-wrapper");\
      busy_msg.removeClass("hide");\
      busy_msg[0].scrollIntoView();\
      $(".modal-body").scrollLeft(0);\
      tch.loadModal(\'modals/wireless-modal.lp?radio='); ngx.print(content.device); ngx.print('&iface='); ngx.print(getiface); ngx.print('\',\'\',function() {\
        $(".modal").modal();\
      });\
    });\
    $("#btn-print").click(function() {\
      let prtwin = window.open("","PRINT","height=600,width=800");\
      prtwin.document.write("<html><body>");\
      prtwin.document.write("<style type=\'text/css\' media=\'print\'>.hide{display:none;} #print-body{-webkit-print-color-adjust:exact;color-adjust:exact;}</style>");\
      prtwin.document.write(document.getElementById("printable").innerHTML);\
      prtwin.document.write("</body></html>");\
      prtwin.document.close();\
      setTimeout(function(){\
        prtwin.print();\
        prtwin.close();\
      },1000);\
    });\
  });\
</script>\
');
