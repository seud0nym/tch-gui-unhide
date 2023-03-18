$("#devices .maccell").each(function(index,cell){
  var vendor=$("#"+cell.id+" .macvendor");
  if (vendor.text() === ""){
    var mac=$("#"+cell.id+" .macaddress").text();
    $.post("/ajax/vendor.lua?mac="+mac,[tch.elementCSRFtoken()],function(data){
      vendor.text(data["name"]);
    },"json")
    .fail(function(response){
      vendor.text("Vendor lookup failed :-(");
    });
  };
});
$("#Refresh_id").html("<i class=\'icon-trash\'></i> Clear MAC cache").removeClass("modal-action-refresh").click(function(){
  tch.loadModal($(".modal form").attr("action")+"&cache=clear");
});
