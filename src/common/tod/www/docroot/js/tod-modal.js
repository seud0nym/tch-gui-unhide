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
