$(function () {
    var opt = {
      theme: 'android-ics light',
      display: 'bubble',
      mode: 'scroller',
      headerText: false,
      timeFormat: 'HH:ii',
      stepMinute: 15
    };
    $("#starttime").mobiscroll().time(opt);
    $("#stoptime").mobiscroll().time(opt);
});
$(".btn-table-add,.btn-table-modify").click(function() {
    // ui_helper adds dummy hidden checkbox if no checkboxes are checked. we don't need that.
    if ($("form .additional-edit input[name=weekdays]:visible:checked").length > 0) {
      $("form input[name=weekdays]:hidden").removeAttr("name");
    } else {
      // we need one checkbox when no checkboxes are checked.
      $("form input[name=weekdays]:hidden").removeAttr("name");
      $("form input[type=checkbox]:hidden:last").attr("name","weekdays");
    }
});
