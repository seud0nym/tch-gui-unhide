--pretranslated: do not change this file

gettext.textdomain('webui-core')

ngx.print('\
<div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>\
<script>\
  function postAction(action,idx,name) {\
    if (confirm("Are you really sure that you want to "+action+" "+name+"?")) {\
      var busy_msg = $(".loading-wrapper");\
      busy_msg.removeClass("hide");\
      busy_msg[0].scrollIntoView();\
      $(".modal-body").scrollLeft(0);\
      var t = $(".modal form"), e = t.serializeArray();\
      e.push({\
        name: "action",\
        value: action+"_"+idx\
      }, {\
        name: "CSRFtoken",\
        value: $("meta[name=CSRFtoken]").attr("content")\
      });\
      tch.loadModal(t.attr("action"), e)\
    }\
  }\
</script>\
')