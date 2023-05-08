var refreshTimeOut = 5000;
var refreshDelay = 3000;
var target = $(".modal form").attr("action");
function wait_for_webserver_running() {
  console.log("wait_for_webserver_running")
  $.ajax({ url: "/gateway.lp",timeout: refreshTimeOut })
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
  console.log("wait_for_webserver_down")
  $.ajax({ url: target,timeout: refreshTimeOut })
  .done(function() {
    window.setTimeout(wait_for_webserver_down,refreshDelay);
  })
  .fail(function() {
    window.setTimeout(wait_for_webserver_running,refreshDelay);
  });
}
function cancel_request(action) {
  $("#"+action+"-confirming-msg").addClass("hide");
  $("#"+action+"-changes").addClass("hide");
  $("#"+action+"-rebooting-msg").addClass("hide");
  $("input#bridged_dhcp").parents("div.control-group").addClass("hide");
}
function confirm_request(action) {
  $("#"+action+"-confirming-msg").removeClass("hide");
  $("#"+action+"-changes").removeClass("hide");
  $("input#bridged_dhcp").parents("div.control-group.hide").removeClass("hide");
  $(".modal-body").animate({"scrollTop":"+=100px"},"fast")
}
function reconfigure(action) {
  let msg = $("#"+action+"-rebooting-msg")
  let button = $("#btn-"+action+"")
  $("#"+action+"-confirming-msg").addClass("hide");
  $("#"+action+"-changes").addClass("hide");
  button.addClass("hide");
  button.after(msg);
  msg.removeClass("hide");
  msg[0].scrollIntoView();
  $(".btn-close").addClass("hide");
  $("#modal-no-change").addClass("hide");
  $.post(
    target,
    { 
      "action":action,
      "dhcp":$("input#bridged_dhcp").val(),
      "CSRFtoken":$("meta[name=CSRFtoken]").attr("content") 
    },
    function(data) {
      if(data.success) {
        wait_for_webserver_down();
      } else {
        msg.text(data.message);
        msg.removeClass("alert");
        msg.addClass("alert alert-error");
        $(".btn-close").removeClass("hide");
        $("#modal-no-change").removeClass("hide");
      }
    },
    "json"
  );
  return false;
}
$("#btn-bridged").click(function() {
  confirm_request("bridged")
});
$("#bridged-confirm").click(function() {
  return reconfigure("bridged");
});
$("#bridged-cancel").click(function() {
  cancel_request("bridged")
});
$("#btn-routed").click(function() {
  confirm_request("routed")
});
$("#routed-confirm").click(function() {
  return reconfigure("routed");
});
$("#routed-cancel").click(function() {
  cancel_request("routed")
});
