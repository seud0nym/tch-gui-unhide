--pretranslated: do not change this file

gettext.textdomain('webui-core')

ngx.print('\
<div class="control-group" id="searchControlGroup">\
  <label class="control-label">Filter</label>\
  <div class="controls">\
    <input data-type="search" name="Filter" class="edit-input span3 no-save" placeholder="Search For Packages..." id="searchInput" type="text">\
  </div>\
</div>\
<script>\
  $("#searchControlGroup").addClass("hide");\
  (function() {\
    $("#searchInput").keyup(function () {\
      var data = this.value.toUpperCase().split(" ");\
      var jo = $("#opkgs tbody").find("tr");\
      if (this.value == "") {\
          jo.show();\
          return;\
      }\
      jo.hide();\
      jo.filter(function (i,v) {\
          var $t = $(this);\
          for (var d = 0; d < data.length; ++d) {\
              if ($t.text().toUpperCase().indexOf(data[d]) > -1) {\
                  return true;\
              }\
          }\
          return false;\
      })\
      .show();\
    }).focus(function () {\
      this.value = "";\
      $(this).css({\
          "color": "black"\
      });\
      $(this).unbind("focus");\
    }).css({\
      "color": "#C0C0C0"\
    });\
  }());\
</script>\
<div id="opkgs_place_holder">\
  <div style="text-align:center;">\
    <img src="/img/loading.gif"/>\
  </div>\
</div>\
')