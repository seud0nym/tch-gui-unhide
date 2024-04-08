$("#devices .maccell").each(function(index,cell){
  var vendor=$("#"+cell.id+" .macvendor");
  if (vendor.text() === ""){
    var mac=$("#"+cell.id+" .macaddress").text();
    $.post("/ajax/vendor.lua?mac="+mac,[tch.elementCSRFtoken()],function(data){
      vendor.text(data["name"]);
    },"json")
    .fail(function(response){
      vendor.text("Vendor lookup failed :-(");
    });
  };
});
$("#Refresh_id").html("<i class=\'icon-trash\'></i> Clear MAC cache").removeClass("modal-action-refresh").click(function(){
  tch.loadModal($(".modal form").attr("action")+"&cache=clear");
});
$("tr.line-edit>td>span.ipv4cell").each(function(){
  var v = $(this).text();
  $(this).parent().append('<input type="hidden" value="'+v+'" name="IPv4" id="IPv4">');
});
$("tr.line-edit>td>span.maccell").each(function(){
  var v = $(this).children('span.macaddress').text();
  $(this).append('<input type="hidden" value="'+v+'" name="MACAddress" id="MACAddress">');
});
$("tr.line-edit>td>span.typecell").each(function(){
  var state = $(this).text();
  var switchClass = "switch";
  var switcherClass = "switcher"
  var v = "0";
  if (state == "Static") {
    switchClass += " switchOn";
    switcherClass += " switcherOn";
    v = "1";
  }
  $(this).parent().prepend('<style>span.typecell .switcher:before,span.typecell .switcher:after{font-size:6px}</style>');
  $(this).html('<div class="control-group"><div class="'+switchClass+'"><div class="'+switcherClass+'" textON="Static" textOFF="DHCP" valOn="1" valOff="0"></div><input type="hidden" value="'+v+'" name="static" id="static"></div></div>')
});
