window.intervalIDs=new Array();
function clearRegisteredIntervals(){
  for(let i=0;i<window.intervalIDs.length;i++){
    clearInterval(window.intervalIDs[i]);
  }
}
var modalLoaded=false;
$(document).bind("DOMSubtreeModified",function(){
  if(!modalLoaded){
    if($(".modal-backdrop").length>0){
      $("#waiting:not(.do-not-show)").fadeOut();
      modalLoaded=true;
      clearRegisteredIntervals();
    }
  }
});
function unhideWaiting(){
  if ($(this).attr("data-remote")||$(this).find("[data-remote]").length>0){
    $("#waiting").fadeIn();
  }
}
$("body").on("click",".smallcard .header,.modal-link",unhideWaiting);
$(document).ajaxComplete(function( event, xhr, settings ) {
  if(xhr.responseText.search("modal-link")!=-1){
    $("body").off("click",".smallcard .header,.modal-link",unhideWaiting).on("click",".smallcard .header,.modal-link",unhideWaiting);
  }
});