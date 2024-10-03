function addOpt(unit,selected){
  var val = unit.toLowerCase();
  var opt = "<option value='"+val+"'";
  if (val == selected) {
    opt += " selected";
  }
  opt += ">"+unit+"</option>";
  return opt;
}
function addUnitSelect(prefix,dfltInterval,dfltUnit){
  var i = $("#"+prefix+"Interval");
  if (i.val() == undefined || i.val() == "") { 
    i.val(dfltInterval); 
  }
  var c = i.parent();
  var u = $("input[name="+prefix+"Unit]");
  var v = u.val();
  if (v == undefined || v == "") { 
    v = dfltUnit; 
  }
  var s = "<select name='"+prefix+"Unit' id='"+prefix+"' class='edit-input' style='width:95px;'>"+
      addOpt("Seconds",v)+
      addOpt("Minutes",v)+
      addOpt("Hours",v)+
      addOpt("Days",v)+
    "</select>"
  c.append(s);
  u.remove();
}
$("#btn-service").on("click",function(){
  var busy_msg = $(".loading-wrapper");
  busy_msg.removeClass("hide");
  busy_msg[0].scrollIntoView();
  var t = $(".modal-body form"),
      e = t.serializeArray();
  e.push({
    name: "action",
    value: "restart_ddns"
  },{
    name: "fromModal",
    value: "YES"
  },{
    name: "CSRFtoken",
    value: $("meta[name=CSRFtoken]").attr("content")
  });
  tch.loadModal(t.attr("action"),e,function(){
    busy_msg.addClass("hide");
  });
  return false;
});
$(".loading-wrapper").addClass("hide");
