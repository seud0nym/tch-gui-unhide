var tFuncID;
var waitingForTelephonyStatusResponse = false;
function updateTelephonyCard(){
  if (waitingForTelephonyStatusResponse){
    console.log("waitingForTelephonyStatusResponse");
    return;
  }
  waitingForTelephonyStatusResponse = true;
  $.post("/ajax/telephony-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#telephony-card-content").html(data["html"]);
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(tFuncID);}
  })
  .always(function() {
    waitingForTelephonyStatusResponse = false;
  });
}
$().ready(function(){
  tFuncID=setInterval(updateTelephonyCard,3000);
  addRegisteredInterval(tFuncID);
});
