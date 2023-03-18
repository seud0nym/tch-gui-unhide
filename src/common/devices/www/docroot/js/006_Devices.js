var dFuncID;
var waitingForDeviceStatusResponse = false;
var updateDevicesCardSkipped = 3;
function updateDevicesCard(){
  if((updateDevicesCardSkipped < 4 && window.activeXHR.length > 2) || waitingForDeviceStatusResponse){
    updateDevicesCardSkipped ++;
    return;
  }
  waitingForDeviceStatusResponse = true;
  updateDevicesCardSkipped = 0;
  $.post("/ajax/devices-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#devices-card-content").html(data["html"]);
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(dFuncID);}
  })
  .always(function() {
    waitingForDeviceStatusResponse = false;
  });
}
$().ready(function(){
  dFuncID=setInterval(updateDevicesCard,3000);
  addRegisteredInterval(dFuncID);
});
