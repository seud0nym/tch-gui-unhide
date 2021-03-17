--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local format, untaint = string.format, string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local ipv = {
  {"4", T"IPv4"},
  {"6", T"IPv6"},
}
local ipv_checked = {
  "4",
  "6",
}

local iptables = proxy.get("rpc.gui.firewall.iptables.")
local ip6tables = proxy.get("rpc.gui.firewall.ip6tables.")

ngx.print(ui_helper.createHeader(T"Firewall", true, false))   ngx.print('\
<div class="modal-body update" id="fw-pkt-filter-rules">\
'); 
  lp.include("tabs-firewall.lp");
  ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-iptables-modal.lp">\
    <div class="control-group">\
      <label class="control-label">Filter</label>\
      <div class="controls">\
        <input data-type="search" name="Filter" class="edit-input span3 no-save" placeholder="Search Rules..." id="searchInput" type="text">\
      </div>\
    </div>\
    <div class="control-group">\
      <label class="control-label">Show</label>\
      <div class="controls">\
        <label class="checkbox inline"><input type="checkbox" id="showIPv4" checked="" class="no-save"> IPv4</label>\
        <label class="checkbox inline"><input type="checkbox" id="showIPv6" checked="" class="no-save"> IPv6</label>\
      </div>\
    </div>\
  </form>\
  <fieldset id="fw-iptables">\
    '); 
    ngx.print('<legend>');  ngx.print( T"IPv4 Packet Filter Rules" ); ngx.print('</legend>');
    ngx.print("<table id='iptables' class='table table-striped'>")
    for _,v in pairs(iptables) do
      ngx.print(untaint(v.value))
    end
    ngx.print("</table>")
    ngx.print('\
  </fieldset>\
  <fieldset id="fw-ip6tables">\
    '); 
    ngx.print('<legend>');  ngx.print( T"IPv6 Packet Filter Rules" ); ngx.print('</legend>');
    ngx.print("<table id='ip6tables' class='table table-striped'>")
    for _,v in pairs(ip6tables) do
      ngx.print(untaint(v.value))
    end
    ngx.print("</table>")
    ngx.print('\
  </fieldset>\
</div>\
');  ngx.print(ui_helper.createFooter()); ngx.print('\
<script>\
  (function() {\
    $("#showIPv4").change(function(){\
      if (this.checked){\
        $("#fw-iptables").show();\
      } else {\
        $("#fw-iptables").hide();\
      }\
    });\
    $("#showIPv6").change(function(){\
      if (this.checked){\
        $("#fw-ip6tables").show();\
      } else {\
        $("#fw-ip6tables").hide();\
      }\
    });\
    $("#searchInput").keyup(function(){\
      var bodies = $("#fw-pkt-filter-rules tbody");\
      var rows = $("#fw-pkt-filter-rules tbody>tr:not(.iptables-head");\
      if (this.value == "") {\
        bodies.show();\
        rows.show();\
        return;\
      }\
      var strings = this.value.toUpperCase().split(" ");\
      rows.hide();\
      rows.filter(function(i, v) {\
        var $t = $(this);\
        for (var c = 0; c < strings.length; ++c) {\
          if ($t.text().toUpperCase().indexOf(strings[c]) > -1) {\
            return true;\
          }\
        }\
        return false;\
      }).show();\
      bodies.show();\
      bodies.filter(function(i, v) {\
        var $t = $(this);\
        if ($t.children("tr:not(.iptables-chain,.iptables-head):visible").length == 0) {\
          return true;\
        }\
        return false;\
      }).hide();\
    }).focus(function () {\
      this.value = "";\
      $(this).css({\
        "color": "black"\
      });\
      $(this).unbind(\'focus\');\
    }).css({\
      "color": "#C0C0C0"\
    });\
  }());\
</script>\
');
