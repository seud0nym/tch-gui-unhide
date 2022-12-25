var tFuncID;
var waitingForTelephonyStatusResponse = false;
var updateTelephonyCardSkipped = 0;
function updateTelephonyCard(){
  if((updateTelephonyCardSkipped < 2 && window.activeXHR.length > 2) || waitingForTelephonyStatusResponse){
    updateTelephonyCardSkipped ++;
    return;
  }
  waitingForTelephonyStatusResponse = true;
  updateTelephonyCardSkipped = 0;
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
