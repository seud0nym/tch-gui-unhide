$(".upload-firmware").click(function() {
  let nofile_msg = $("#upload-nofile-msg");
  if ($("#file-upload").val() == "") {
    nofile_msg.removeClass("hide");
    nofile_msg[0].scrollIntoView();
    return false;
  }
  nofile_msg.addClass("hide");

  let _this = $(this).parents(".control-group");
  $("#upload-failed-msg").addClass("hide");
  let uploading_msg = $("#uploading-msg");
  uploading_msg.removeClass("hide");
  uploading_msg[0].scrollIntoView();

  let busy_msg = $(".loading-wrapper");
  busy_msg.removeClass("hide");

  $.fileUpload($("#form-upload"), {
    params: { 
      CSRFtoken: $("meta[name=CSRFtoken]").attr("content")
    },
    completeCallback: function(form, response) {
      $("#uploading-msg").addClass("hide");
      if (response.success) {
        let msg = $("#rebooting-msg");
        let msg_dst = $(_this);
        msg_dst.after(msg);
        msg.removeClass("hide");
        tch.loadModal($(".modal form").attr("action"));
      }
      else {
        $("#upload-failed-msg").removeClass("hide");
      }
    }
  });
  return false;
});
