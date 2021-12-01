--pretranslated: do not change this file

gettext.textdomain('webui-core')

ngx.print('\
<div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>\
<script>\
  $("#save-config").on("click",function(){\
    if (confirm("Are you sure?")) {\
      var busy_msg = $(".loading-wrapper");\
      busy_msg.removeClass("hide");\
      busy_msg[0].scrollIntoView();\
      $(".modal-body").scrollLeft(0);\
      return true;\
    }\
    return false;\
  });\
</script>\
')