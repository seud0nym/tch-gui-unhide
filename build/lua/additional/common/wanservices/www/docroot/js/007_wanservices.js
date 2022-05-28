var wanservicesFuncID;
function updateWANServicesCard(){
  $.post("/ajax/wanservices-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#wanservices-card-content").html(data["html"]);
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(wanservicesFuncID);}
  });
}
setTimeout(updateWANServicesCard,0);
$().ready(function(){
  wanservicesFuncID=setInterval(updateWANServicesCard,21000);
  addRegisteredInterval(wanservicesFuncID);
});
