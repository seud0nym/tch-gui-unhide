var ssidFuncID;
var waitingForSSIDStatusResponse = false;
var updateWirelessCardSkipped = 2;
function updateWirelessCard(){
  if((updateWirelessCardSkipped < 2 && window.activeXHR.length > 2) || waitingForSSIDStatusResponse){
    updateWirelessCardSkipped ++;
    return;
  }
  waitingForSSIDStatusResponse = true;
  updateWirelessCardSkipped = 0;
  $.post("/ajax/ssid-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#wifi-card-content").html(data["html"]);
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(ssidFuncID);}
  })
  .always(function() {
    waitingForSSIDStatusResponse = false;
  });
}
$().ready(function(){
  ssidFuncID=setInterval(updateWirelessCard,19000);
  addRegisteredInterval(ssidFuncID);
});
