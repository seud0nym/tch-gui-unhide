var iFuncID;
var waitingForInternetStatusResponse = false;
function updateInternetCard(){
  if (waitingForInternetStatusResponse){
    console.log("waitingForInternetStatusResponse");
    return;
  }
  waitingForInternetStatusResponse = true;
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
  setTimeout(updateInternetCard,0);
  iFuncID=setInterval(updateInternetCard,20000);
  addRegisteredInterval(iFuncID);
});
