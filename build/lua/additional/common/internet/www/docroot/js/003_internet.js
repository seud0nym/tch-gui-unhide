var iFuncID;
function updateInternetCard(){
  $.post("/ajax/internet-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#internet-card-content").html(data["html"]);
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(iFuncID);}
  });
}
setTimeout(updateInternetCard,0);
$().ready(function(){
  iFuncID=setInterval(updateInternetCard,20000);
  window.intervalIDs.push(iFuncID);
});
