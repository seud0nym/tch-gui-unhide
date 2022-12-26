var iFuncID;
var waitingForInternetStatusResponse = false;
var updateInternetCardSkipped = 2;
function updateInternetCard(){
  if((updateInternetCardSkipped < 2 && window.activeXHR.length > 2) || waitingForInternetStatusResponse){
    updateInternetCardSkipped ++;
    return;
  }
  waitingForInternetStatusResponse = true;
  updateInternetCardSkipped = 0;
  $.post("/ajax/internet-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#internet-card-content").html(data["html"]);
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(iFuncID);}
  })
  .always(function() {
    waitingForInternetStatusResponse = false;
  });
}
$().ready(function(){
  iFuncID=setInterval(updateInternetCard,20000);
  addRegisteredInterval(iFuncID);
});
