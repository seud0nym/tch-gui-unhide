--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local lp = require("web.lp")
local ui_helper = require("web.ui_helper")
ngx.print( ui_helper.createHeader(T"Devices", false, true, 5) ); ngx.print('\
<div class="modal-body">\
');  
  lp.setpath("/www/snippets/")
  lp.include("tabs-devices.lp")
  lp.include("networkmap.lp")
  ngx.print('\
</div>\
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
<script>\
$("#networkmap a").click(function (e) {\
  var e=e||window.event;\
  e.stopPropagation();\
  e.preventDefault();\
});\
</script>\
'); 
