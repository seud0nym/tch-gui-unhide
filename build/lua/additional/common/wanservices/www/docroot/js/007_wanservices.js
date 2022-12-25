var wanservicesFuncID;
var waitingForWanServicesStatusResponse = false;
var updateWANServicesCardSkipped = 0;
function updateWANServicesCard(){
  if((updateWANServicesCardSkipped < 2 && window.activeXHR.length > 2) || waitingForWanServicesStatusResponse){
    updateWANServicesCardSkipped ++;
    return;
  }
  waitingForWanServicesStatusResponse = true;
  updateWANServicesCardSkipped = 0;
  $.post("/ajax/wanservices-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#wanservices-card-content").html(data["html"]);
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(wanservicesFuncID);}
  })
  .always(function() {
    waitingForWanServicesStatusResponse = false;
  });
}
$().ready(function(){
  wanservicesFuncID=setInterval(updateWANServicesCard,21000);
  addRegisteredInterval(wanservicesFuncID);
});
