var multiapFuncID;
var waitingForBoosterStatusResponse = false;
function updateBoosterCard(){
  if (waitingForBoosterStatusResponse){
    console.log("waitingForBoosterStatusResponse");
    return;
  }
  waitingForBoosterStatusResponse = true;
  $.post("/ajax/booster-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#booster-card-content").html(data["html"]);
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(multiapFuncID);}
  })
  .always(function() {
    waitingForBoosterStatusResponse = false;
  });
}
$().ready(function(){
  setTimeout(updateBoosterCard,0);
  multiapFuncID=setInterval(updateBoosterCard,20000);
  addRegisteredInterval(multiapFuncID);
});
