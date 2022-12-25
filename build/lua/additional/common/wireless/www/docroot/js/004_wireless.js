var ssidFuncID;
var waitingForSSIDStatusResponse = false;
function updateWirelessCard(){
  if (waitingForSSIDStatusResponse){
    console.log("waitingForSSIDStatusResponse");
    return;
  }
  waitingForSSIDStatusResponse = true;
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
  setTimeout(updateWirelessCard,0);
  ssidFuncID=setInterval(updateWirelessCard,19000);
  addRegisteredInterval(ssidFuncID);
});
