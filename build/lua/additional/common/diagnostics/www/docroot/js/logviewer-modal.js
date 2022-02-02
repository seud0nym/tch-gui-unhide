var target = $(".modal form").attr("action");

$(".export-conntracklog").click(function() {
  $.fileDownload(target,{
    httpMethod: "POST",
    data: new Array({ name:"action",value:"export_log" },{ name:"CSRFtoken",value:$("meta[name=CSRFtoken]").attr("content") }),
    prepareCallback: function() {
      $("#export-failed-msg").addClass("hide");
      var exporting_msg = $("#exporting-msg");
      exporting_msg.removeClass("hide");
      exporting_msg[0].scrollIntoView();
    },
    successCallback: function() {
      $("#exporting-msg").addClass("hide");
    },
    failCallback: function() {
      var export_failed_msg = $("#export-failed-msg");
      export_failed_msg.removeClass("hide");
      export_failed_msg[0].scrollIntoView();
      $("#exporting-msg").addClass("hide");
    }
  });
  return false;
});

$('select[name="process"]').on("change",function() {
  var process = $(this).val();
  tch.loadModal("/modals/logviewer-modal.lp?process=" + process)
});
