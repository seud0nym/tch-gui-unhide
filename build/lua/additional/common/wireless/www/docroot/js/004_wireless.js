var ssidFuncID;
function updateWirelessCard(){
  $.post("/ajax/ssid-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#wifi-card-content").html(data["html"]);
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(ssidFuncID);}
  });
}
setTimeout(updateWirelessCard,0);
$().ready(function(){
  ssidFuncID=setInterval(updateWirelessCard,19000);
  addRegisteredInterval(ssidFuncID);
});
