(function() {
  $("#showIPv4").change(function(){
    if (this.checked){
      $("#iptables-filter,#iptables-nat,#iptables-mangle,#iptables-raw").show();
    } else {
      $("#iptables-filter,#iptables-nat,#iptables-mangle,#iptables-raw").hide();
    }
  });
  $("#showIPv6").change(function(){
    if (this.checked){
      $("#ip6tables-filter,#ip6tables-mangle,#ip6tables-raw").show();
    } else {
      $("#ip6tables-filter,#ip6tables-mangle,#ip6tables-raw").hide();
    }
  });
  $("#searchInput").keyup(function(){
    var bodies = $("#fw-iptables tbody");
    var rows = $("#fw-iptables tbody>tr:not(.iptables-head");
    if (this.value == "") {
      bodies.show();
      rows.show();
      return;
    }
    var strings = this.value.toUpperCase().split(" ");
    rows.hide();
    rows.filter(function(i,v) {
      var $t = $(this);
      for (var c = 0; c < strings.length; ++c) {
        if ($t.text().toUpperCase().indexOf(strings[c]) > -1) {
          return true;
        }
      }
      return false;
    }).show();
    bodies.show();
    bodies.filter(function(i,v) {
      var $t = $(this);
      if ($t.children("tr:not(.iptables-head):visible").length == 0) {
        return true;
      }
      if ($t.children("tr.iptables-chain:visible").length > 0) {
        $t.children("tr").show();
      } else {
        $t.children("tr.iptables-chain").show();
      }
    return false;
    }).hide();
  }).focus(function () {
    this.value = "";
    $(this).css({
      "color": "black"
    });
    $(this).unbind('focus');
  }).css({
    "color": "#C0C0C0"
  });
}());
