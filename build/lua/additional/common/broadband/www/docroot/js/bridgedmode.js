var refreshTimeOut = 5000;
var refreshDelay = 3000;
var target = $(".modal form").attr("action");
function wait_for_webserver_running() {
  $.ajax({ url: "/",timeout: refreshTimeOut })
  .done(function(data) {
    document.open("text/html");
    document.write(data);
    document.close();
  })
  .fail(function() {
    window.setTimeout(wait_for_webserver_running,refreshDelay);
  });
}
function wait_for_webserver_down() {
  $.ajax({ url: target,timeout: refreshTimeOut })
  .done(function() {
    window.setTimeout(wait_for_webserver_down,refreshDelay);
  })
  .fail(function() {
    window.setTimeout(wait_for_webserver_running,refreshDelay);
  });
}
function resetreboot(msg,msg_dst,action) {
  msg_dst.after(msg);
  msg.removeClass("hide");
  msg[0].scrollIntoView();
  $.post(
    target,
    { action: action,CSRFtoken: $("meta[name=CSRFtoken]").attr("content") },
    wait_for_webserver_down,
    "json"
  );
  return false;
}
$("#btn-bridged").click(function() {
  $("#bridged-confirming-msg").removeClass("hide");
  $("#bridged-changes").removeClass("hide");
  $(".modal-body").animate({"scrollTop":"+=100px"},"fast")
});
$("#bridged-confirm").click(function() {
  $("#bridged-confirming-msg").addClass("hide");
  $("#bridged-changes").addClass("hide");
  $("#btn-bridged").addClass("hide");
  return resetreboot($("#bridged-rebooting-msg"),$("#btn-bridged"),"BRIDGED");
});
$("#bridged-cancel").click(function() {
  $("#bridged-confirming-msg").addClass("hide");
  $("#bridged-changes").addClass("hide");
  $("#bridged-rebooting-msg").addClass("hide");
});
$("#btn-routed").click(function() {
  $("#routed-confirming-msg").removeClass("hide");
  $("#routed-changes").removeClass("hide");
  $(".modal-body").animate({"scrollTop":"+=100px"},"fast")
});
$("#routed-confirm").click(function() {
  $("#routed-confirming-msg").addClass("hide");
  $("#routed-changes").addClass("hide");
  $("#btn-routed").addClass("hide");
  return resetreboot($("#routed-rebooting-msg"),$("#btn-routed"),"ROUTED");
});
$("#routed-cancel").click(function() {
  $("#routed-confirming-msg").addClass("hide");
  $("#routed-changes").addClass("hide");
  $("#routed-rebooting-msg").addClass("hide");
});
