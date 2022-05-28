window.activeXHR=new Array();
window.intervalIDs=new Array();
function abortActiveAjaxRequests(){
  for(let i=0;i<window.activeXHR.length;i++){
    window.activeXHR[i].abort();
  }
  window.activeXHR=new Array();
}
function addRegisteredInterval(id){
  if(window.autoRefreshEnabled){
    window.intervalIDs.push(id);
  } else{
    clearInterval(id);
  }
}
function clearRegisteredIntervals(){
  for(let i=0;i<window.intervalIDs.length;i++){
    clearInterval(window.intervalIDs[i]);
  }
  window.intervalIDs=new Array();
}
function unhideWaiting(){
  if ($(this).attr("data-remote")||$(this).find("[data-remote]").length>0){
    $("#waiting").fadeIn();
  }
}
let modalLoaded=false;
$(document).bind("DOMSubtreeModified",function(){
  if(!modalLoaded){
    if($(".modal-backdrop").length>0){
      modalLoaded=true;
      $("#waiting:not(.do-not-show)").fadeOut();
      setTimeout(clearRegisteredIntervals,1);
      setTimeout(abortActiveAjaxRequests,1);
    }
  }
});
$("body").on("click",".smallcard .header,.modal-link",unhideWaiting);
$(document).ajaxSend(function(event,xhr,settings) {
  window.activeXHR.push(xhr);
});
$(document).ajaxComplete(function(event,xhr,settings) {
  if(xhr.responseText!==undefined && xhr.responseText.search("modal-link")!=-1){
    $("body").off("click",".smallcard .header,.modal-link",unhideWaiting).on("click",".smallcard .header,.modal-link",unhideWaiting);
  }
  let indexOfXHR = window.activeXHR.findIndex(object => {
    return object === xhr;
  });
  if(indexOfXHR!=-1){
    window.activeXHR.splice(indexOfXHR,1);
  }
});