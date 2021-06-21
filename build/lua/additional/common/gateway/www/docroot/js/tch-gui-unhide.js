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
