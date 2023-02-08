(function() {
  $("#searchInput").keyup(function () {
    var data = this.value.toUpperCase().split(" ");
    var jo = $("#proctable tbody").find("tr");
    if (this.value == "") {
      jo.show();
      return;
    }
    jo.hide();
    jo.filter(function (i,v) {
      var $t = $(this);
      for (var d = 0; d < data.length; ++d) {
        if ($t.text().toUpperCase().indexOf(data[d]) > -1) {
          return true;
        }
      }
      return false;
    })
    .show();
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
