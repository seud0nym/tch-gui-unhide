--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local getargs = ngx.req.get_uri_args()
if getargs.start and getargs.start == "true" then
  proxy.set("rpc.gui.speedtest.running","1")
  proxy.apply()
  ngx.sleep(30)
  while proxy.get("rpc.gui.speedtest.running")[1] == "1" do
    ngx.sleep(5)
  end
end

local result_columns = {
  {
    header = T"Time Stamp",
    name = "timestamp",
    param = "timestamp",
    readonly = true,
  },
  {
    header = T"Download<br>(Mbps)",
    name = "download",
    param = "download",
    readonly = true,
  },
  {
    header = T"Upload<br>(Mbps)",
    name = "upload",
    param = "upload",
    readonly = true,
  },
  {
    header = T"Ping<br>(ms)",
    name = "latency",
    param = "latency",
    readonly = true,
  },
  {
    header = T"Download<br>Latency (ms)",
    name = "download_latency",
    param = "download_latency",
    readonly = true,
  },
  {
    header = T"Upload<br>Latency (ms)",
    name = "upload_latency",
    param = "upload_latency",
    readonly = true,
  },
  {
    header = T"Packet<br>Loss (%)",
    name = "packet_loss",
    param = "packet_loss",
    readonly = true,
  },
  {
    header = T"ISP",
    name = "isp",
    param = "isp",
    readonly = true,
  },
  {
    header = T"Server",
    name = "server",
    param = "server",
    readonly = true,
  },
  {
    header = T"URL",
    name = "url",
    param = "url",
    readonly = true,
  },
  {
    header = T"Message",
    name = "error_message",
    param = "error_message",
    readonly = true,
  },
}
local c = {}
for i,v in ipairs(result_columns) do
  c[v.name] = i
end

local result_options = {
  tableid = "results",
  basepath = "rpc.gui.speedtest.result.@.",
  canAdd = false,
  canDelete = false,
  canEdit = false,
}

local result_filter = function(data)
  local url = untaint(data.url)
  local id = string.match(url,"([%x%-]+)$")
  data.timestamp = os.date("%c",tonumber(untaint(data.timestamp)))
  data.url = "<a style='font-size:9px' target='_blank' href='"..url.."'>"..id.."&nbsp;<i class='icon-external-link'></i></a>"
  return true
end

local result_data,result_helpmsg = post_helper.handleTableQuery(result_columns,result_options,result_filter,nil,nil)
local latest = table.remove(result_data,1)

ngx.print(ui_helper.createHeader(T"Diagnostics", false, false) ); ngx.print('\
<div class="svg-region" style="display:none;">\
  <svg xmlns="http://www.w3.org/2000/svg" preserveAspectRatio="xMinYMin meet" viewBox="0 0 0 0" style="display: none;">\
    <symbol id="icon-download" viewBox="0 0 24.01 23.99">\
      <path d="M54.26,198.5a12,12,0,1,0,12,12A12,12,0,0,0,54.26,198.5Zm0,22a10,10,0,1,1,10-10A10,10,0,0,1,54.28,220.5Zm5-11a1,1,0,0,0-.71.29l-3.3,3.3V205.5a1,1,0,0,0-2,0v7.59L50,209.8a1,1,0,1,0-1.42,1.42l5,5a1,1,0,0,0,1.42,0l5-5A1,1,0,0,0,59.28,209.5Z" transform="translate(-42.26 -198.5)"></path>\
    </symbol>\
    <symbol id="icon-ping" viewBox="0 0 24.01 23.99">\
      <path d="M48.5,169.61a1,1,0,0,0,.77.39h10a1,1,0,0,0,0-2H51.19l3.06-3.06a1,1,0,1,0-1.41-1.41l-4.54,4.54a1,1,0,0,0,0,1.41A1,1,0,0,0,48.5,169.61ZM54.26,159a12,12,0,1,0,12,12A12,12,0,0,0,54.26,159Zm0,22a10,10,0,1,1,10-10A10,10,0,0,1,54.28,181ZM60,172.39a1,1,0,0,0-.77-0.39h-10a1,1,0,1,0,0,2h8.09l-3.06,3.06a1,1,0,1,0,1.41,1.41l4.54-4.54a1,1,0,0,0,0-1.41A1,1,0,0,0,60,172.39Z" transform="translate(-42.26 -159.01)"></path>\
    </symbol>\
    <symbol id="icon-upload" viewBox="0 0 24.01 23.99">\
      <path d="M54.26,238a12,12,0,1,0,12,12A12,12,0,0,0,54.26,238Zm0,22a10,10,0,1,1,10-10A10,10,0,0,1,54.28,260ZM55,244.28a1,1,0,0,0-1.42,0l-5,5A1,1,0,0,0,50,250.7l3.3-3.3V255a1,1,0,0,0,2,0v-7.59l3.3,3.3A1,1,0,0,0,60,249.29Z" transform="translate(-42.26 -238)"></path>\
    </symbol>\
  </svg>\
  <svg xmlns="http://www.w3.org/2000/svg" id="logo_speedtest" viewBox="0 0 193 24" height="24">\
    <path d="M35.7 19.3l1.9-2.3c1.7 1.5 3.5 2.4 5.8 2.4 2 0 3.2-.9 3.2-2.3V17c0-1.3-.7-2-4.1-2.8-3.9-.9-6.1-2.1-6.1-5.5 0-3.1 2.6-5.3 6.3-5.3 2.7 0 4.8.8 6.7 2.3l-1.7 2.4c-1.6-1.2-3.3-1.9-5-1.9-1.9 0-3 1-3 2.2v.1c0 1.4.8 2.1 4.4 2.9 3.9.9 5.9 2.3 5.9 5.4v.1c0 3.4-2.7 5.5-6.6 5.5-2.9-.2-5.5-1.1-7.7-3.1m23.9-6.1c2.4 0 3.9-1.4 3.9-3.3 0-2.2-1.5-3.3-3.9-3.3h-3.7v6.6h3.7zm-7-9.5h7.2c4.3 0 7 2.4 7 6.1v.1c0 4.1-3.3 6.3-7.3 6.3h-3.6V22h-3.2l-.1-18.3zm17 0h13.6v2.8H72.8v4.8H82v2.9h-9.2v4.9h10.5V22H69.6zm17 0h13.6v2.8H89.8v4.8H99v2.9h-9.2v4.9h10.5V22H86.6zm20.2 2.9v12.5h3.6c3.8 0 6.4-2.6 6.4-6.2v-.1c0-3.6-2.5-6.2-6.4-6.2h-3.6zm-3.2-2.9h6.8c5.8 0 9.7 3.9 9.7 9.1v.1c0 5.2-4 9.2-9.7 9.2h-6.8V3.7zm24.2 2.9H122V3.7h14.9v2.9h-5.8V22h-3.3zm12.5-2.9h13.6v2.8h-10.4v4.8h9.2v2.9h-9.2v4.9H154V22h-13.7zm16.3 15.6l1.9-2.3c1.7 1.5 3.5 2.4 5.8 2.4 2 0 3.2-.9 3.2-2.3V17c0-1.3-.7-2-4.1-2.8-3.9-.9-6.1-2.1-6.1-5.5 0-3.1 2.6-5.3 6.3-5.3 2.7 0 4.8.8 6.7 2.3l-1.7 2.4c-1.6-1.2-3.3-1.9-5-1.9-1.9 0-3 1-3 2.2v.1c0 1.4.8 2.1 4.4 2.9 3.9.9 5.9 2.3 5.9 5.4v.1c0 3.4-2.7 5.5-6.6 5.5-2.9-.2-5.5-1.1-7.7-3.1m22.1-12.7h-5.8V3.7h14.9v2.9H182V22h-3.3zM23 23c2.3-2.3 3.8-5.5 3.8-9.1C26.8 6.8 21 1 13.9 1 6.8 1 1 6.8 1 13.9c0 3.6 1.4 6.8 3.8 9.1l1.8-1.8c-1.9-1.9-3-4.4-3-7.3 0-5.7 4.6-10.3 10.3-10.3 5.7 0 10.3 4.6 10.3 10.3 0 2.8-1.2 5.4-3 7.3L23 23z"></path>\
    <path d="M13.5 16.5l-2.2-2.3 7.3-6.4 1.3 1.4zM190.8 5.1c.2 0 .4-.1.4-.3 0-.2-.1-.3-.4-.3h-.5v.6h.5zm-.8-.9h.8c.2 0 .4.1.5.2.1.1.2.2.2.4 0 .3-.2.5-.4.5l.5.7h-.4l-.4-.6h-.4V6h-.4V4.2z"></path>\
    <path d="M190.7 3.6c-.8 0-1.5.7-1.5 1.5s.7 1.5 1.5 1.5 1.5-.7 1.5-1.5-.7-1.5-1.5-1.5m0 .2c.7 0 1.3.6 1.3 1.3 0 .7-.6 1.3-1.3 1.3-.7 0-1.3-.6-1.3-1.3 0-.7.5-1.3 1.3-1.3"></path>\
  </svg>\
</div>\
<style>\
  #logo_speedtest{fill:var(--RGB_TXT)}\
  #results>thead>tr>th:nth-child(n+2):nth-child(-n+7),#results>tbody>tr>td:nth-child(n+2):nth-child(-n+7){text-align:right;width:100px;}\
  .result-container{width:100%;}\
  .result-container-speed{padding-top:10px;background-color:transparent;border-radius:10px;-webkit-transition:background-color 250ms linear;-moz-transition:background-color 250ms linear;-ms-transition:background-color 250ms linear;-o-transition:background-color 250ms linear;transition:background-color 250ms linear;}\
  .result-container-speed-active{background-color:#26273b;}\
  .result-container-flex-center{justify-content:center;}\
  .result-container-data{width:90%;margin-left:5%;display:flex;justify-content:center;gap:4rem;}\
  .result-container-flex-center .result-item-container{margin:0 30px;}\
  .result-container-data .result-item-container{min-width:100px;}\
  .result-item-container-align-right{text-align:right;position:relative;}\
  .result-item-details{text-align:center;padding-bottom:20px;}\
  .result-item-details .result-item{margin-bottom:.3rem;display:inline;margin:0 15px;font-size:14px;font-size:1.1999999998rem;}\
  .result-item{margin-bottom:1em;width:100%;clear:both;}\
  .result-item .result-data, .result-item .result-data-value, .result-item .result-label, .result-item .svg-icon{transition:color linear 250ms;}\
  .result-data-large{font-size:50px;font-size:5rem;}\
  .result-label{font-size:14px;font-size:1.4rem;}\
  .result-data-unit{color:#9193a8;margin-top:-.8em;text-transform:none;}\
  .result-item-download .svg-icon{color:#6afff3;}\
  .result-item-details .result-data-latency-item{margin-left:30px;}\
  .result-item-details .result-data-value{font-variant-numeric:tabular-nums;font-feature-settings:"tnum";}\
  .result-data-latency-item .result-data-value{width:2em;display:inline-block;text-align:left;}\
  .svg-icon{height:16px;width:16px;}\
  .svg-icon-bump{margin:0 .4em;}\
  .result-item-first-class .result-data.u-align-left{padding-left:1.7em;}\
  .result-item-first-class .result-data{display:block;line-height:1.2;background-color:rgba(26,27,46,0);-webkit-transition:all 250ms ease-in-out;-moz-transition:all 250ms ease-in-out;-ms-transition:all 250ms ease-in-out;-o-transition:all 250ms ease-in-out;transition:all 250ms ease-in-out;}\
  .u-align-left{text-align:left;}\
  .result-view .start-button a{transform:translateY(59px) scale(.55555556) translateX(0);}\
  .start-button{display:block;text-align:center;white-space:nowrap;width:calc(100% - 9px - 9px);margin:0;}\
  .start-button a{z-index:1;color:#fff;text-transform:uppercase;text-decoration:none;display:block;margin:0 auto;text-align:center;font-size:29px;font-weight:500;box-sizing:border-box;position:relative;left:0;padding:0;width:100px;height:100px;line-height:102px;border-radius:100px;transform:translateY(18px) translateX(0);transform-origin:center center;border-width:0;}\
  .start-button a .start-border, .result-view .start-button a .start-ring{animation:none;}\
  .start-button a .start-ring{display:block;position:absolute;top:0;left:0;width:100%;height:100%;border-radius:100%;box-sizing:border-box;border:2px #26c5dd solid;opacity:0;animation-name:start-ring;animation-delay:3.5s;animation-duration:3.5s;animation-iteration-count:infinite;animation-timing-function:linear;}\
  .start-button a .start-background{display:block;position:absolute;top:0;left:0;width:100%;height:100%;border-radius:100%;box-sizing:border-box;opacity:0;background-color:#232f4e;}\
  .start-button a .start-border{display:block;position:absolute;top:0;left:0;width:100%;height:100%;border-radius:100%;box-sizing:border-box;background-color:#141526;border:2px transparent solid;background-origin:border-box;background-clip:content-box,border-box;background-image:linear-gradient(#141526,#141526),linear-gradient(to bottom,#2de5d1,#1fa4e9);animation-name:start-heartbeat;animation-delay:3.5s;animation-duration:3.5s;animation-iteration-count:infinite;animation-timing-function:ease-out;}\
  .start-button a .start-text{display:block;position:absolute;top:0;left:0;width:100%;height:100%;border-radius:100%;box-sizing:border-box;white-space:nowrap;opacity:1;}\
  .fake-link, a{cursor:pointer;color:#1cbfff;text-decoration:none;}\
</style>\
<div class="modal-body update">');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-diagnostics.lp")
  ngx.print('\
  <svg height="24" width="200"><use xlink:href="#logo_speedtest"></use></svg>\
  <form class="form-horizontal" method="post" action="modals/diagnostics-speedtest-modal.lp">')
    if latest then
      ngx.print('\
      <div class="result-container-speed">\
        <div class="result-item-details"><div class="result-label">',latest[c.timestamp],'</div>',latest[c.url],'</div>\
        <div class="result-container-data  result-container-flex-center">\
          <div class="result-item-container result-item-container-align-center">\
              <div class="result-item result-item-first-class result-item-download" title="Receiving Time">\
                <div class="result-label">\
                  <svg height="20" fill="#6afff3" class="svg-icon svg-icon-bump"><use xlink:href="#icon-download"></use></svg>\
                  Download\
                  <span class="result-data-unit">Mbps</span>\
                </div>\
                <div class="result-data u-align-left">\
                  <span id="latest-download" class="result-data-large number result-data-value">0.00</span>\
                </div>\
              </div>\
            </div>\
            <div class="result-item-container result-item-container-align-center">\
              <div class="result-item result-item-first-class result-item-upload" title="Sending Time">\
                <div class="result-label">\
                  <svg height="20" fill="#bf71ff" class="svg-icon svg-icon-bump"><use xlink:href="#icon-upload"></use></svg>\
                  Upload\
                  <span class="result-data-unit">Mbps</span>\
                </div>\
                <div class="result-data u-align-left">\
                  <span id="latest-upload" class="result-data-large number result-data-value">0.00</span>\
                </div>\
              </div>\
            </div>\
        </div>\
        <div class="result-item-details">\
          <div class="result-item result-item-ping" title="Reaction Time">\
            <span class="result-label">\
              Ping\
            </span>\
            <span class="result-data-unit">ms</span>\
            <span class="result-data-latency-item" title="Idle Latency">\
              <svg height="20" fill="#fff38e" class="svg-icon svg-icon-bump"><use xlink:href="#icon-ping"></use></svg>\
              <span class="result-data-value">',latest[c.latency],'</span>\
            </span>\
            <span class="result-data-latency-item" title="Download Latency">\
              <svg height="20" fill="#6afff3" class="svg-icon"><use xlink:href="#icon-download"></use></svg>\
              <span class="result-data-value">',latest[c.download_latency],'</span>\
            </span>\
            <span class="result-data-latency-item" title="Upload Latency">\
              <svg height="20" fill="#bf71ff" class="svg-icon"><use xlink:href="#icon-upload"></use></svg>\
              <span class="result-data-value">',latest[c.upload_latency],'</span>\
            </span>\
          </div>\
        </div>\
        <div class="result-item-details">\
          <div class="result-item">\
            <span class="result-label">',latest[c.isp],'<i class="icon-exchange" style="padding:0px 10px;"></i>',latest[c.server],'</span>\
          </div>\
        </div>\
      </div>')
    end
    ngx.print('\
    <div class="start-button">\
      <a id="start-speedtest" href="#" role="button" tabindex="3">\
        <span class="start-ring"></span>\
        <span class="start-background"></span>\
        <span class="start-border"></span>\
        <span class="start-text">Go</span>\
      </a>\
    </div>')
    if #result_data > 0 then
      ngx.print('\
    <fieldset>\
      <legend>',T"Previous Results",'</legend>',
      ui_helper.createTable(result_columns,result_data,result_options,nil,result_helpmsg),'\
    </fieldset>')
    end
    ngx.print('\
  </form>\
</div>\
<div class="loading-wrapper hide"><img src="/img/spinner.gif"/></div>\
<script>')
  if latest then
    ngx.print('\
    function update_mbps(element,max){\
      let v=Number(element.text());\
      v=(v+(max/200)).toFixed(2);\
      if(v>max){\
        v=max.toFixed(2);\
      }\
      element.text(v);\
      if(v<max){\
        setTimeout(update_mbps,0,element,max);\
      }\
    }\
    update_mbps($("#latest-download"),',latest[c.download],');\
    update_mbps($("#latest-upload"),',latest[c.upload],');\
    ')
  end
  ngx.print('\
  $(document).ready(function() {\
    $("#start-speedtest").click(function() {\
      let busy_msg = $(".loading-wrapper");\
      busy_msg.removeClass("hide");\
      busy_msg[0].scrollIntoView();\
      $(".modal-body").scrollLeft(0);\
      tch.loadModal($(".modal form").attr("action")+"?start=true");\
      return false;\
    });\
  });\
</script>\
',ui_helper.createFooter() )
