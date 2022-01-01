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
$(".header input#set_wifi_radio_state").on("change",function(){$("#waiting").removeClass("hide")});
$().ready(function(){
  ssidFuncID=setInterval(updateWirelessCard,19000);
  window.intervalIDs.push(ssidFuncID);
});
