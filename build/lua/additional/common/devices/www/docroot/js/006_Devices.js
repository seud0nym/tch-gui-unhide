var dFuncID;
var waitingForDeviceStatusResponse = false;
function updateDevicesCard(){
  if (waitingForDeviceStatusResponse){
    console.log("waitingForDeviceStatusResponse");
    return;
  }
  waitingForDeviceStatusResponse = true;
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
