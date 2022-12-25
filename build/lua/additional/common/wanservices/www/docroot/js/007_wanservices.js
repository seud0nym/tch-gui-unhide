var wanservicesFuncID;
var waitingForWanServicesStatusResponse = false;
function updateWANServicesCard(){
  if (waitingForWanServicesStatusResponse){
    console.log("waitingForWanServicesStatusResponse");
    return;
  }
  waitingForWanServicesStatusResponse = true;
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
  setTimeout(updateWANServicesCard,0);
  wanservicesFuncID=setInterval(updateWANServicesCard,21000);
  addRegisteredInterval(wanservicesFuncID);
});
