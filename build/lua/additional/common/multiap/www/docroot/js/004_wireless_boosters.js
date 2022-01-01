var multiapFuncID;
function updateBoosterCard(){
  $.post("/ajax/booster-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#booster-card-content").html(data["html"]);
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(multiapFuncID);}
  });
}
setTimeout(updateBoosterCard,0);
$().ready(function(){
  multiapFuncID=setInterval(updateBoosterCard,20000);
  window.intervalIDs.push(multiapFuncID);
});
