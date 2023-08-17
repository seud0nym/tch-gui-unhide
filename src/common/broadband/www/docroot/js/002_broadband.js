var bbFuncID;
var tpFuncID;
var waitingForBroadbandStatusResponse = false;
var waitingForThroughputResponse = false;
var updateBroadbandCardSkipped = 2;
var updateThroughputSkipped = 0;
function broadbandCardIcon(status){
  switch(status){
    case "up":
      return "\uf00c"; // Okay (Tick)
    case "disabled":
      return "\uf05e"; // Ban circle
    case "connecting":
      return "\uf110"; // Spinner
    case "bridged":
      return "\uf0ec"; // Exchange
    default:
      return "\uf071"; // Warning Sign
  }
}
function updateBroadbandCard(){
  if((updateBroadbandCardSkipped < 2 && window.activeXHR.length > 2) || waitingForBroadbandStatusResponse){
    updateBroadbandCardSkipped ++;
    return;
  }
  waitingForBroadbandStatusResponse = true;
  updateBroadbandCardSkipped = 0;
  $.post("/ajax/broadband-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#broadband-card-content").html(data["html"]);
    $("#broadband-card .content").removeClass("mirror").attr("data-bg-text",broadbandCardIcon(data["status"]));
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(bbFuncID);}
  })
  .always(function() {
    waitingForBroadbandStatusResponse = false;
  });
}
function updateThroughput(){
  if((updateThroughputSkipped < 5 && window.activeXHR.length > 2) || waitingForThroughputResponse){
    updateThroughputSkipped ++;
    return;
  }
  waitingForThroughputResponse = true;
  updateThroughputSkipped = 0;
  $.post("/ajax/network-throughput.lua",[tch.elementCSRFtoken()],function(data){
    $("#broadband-card-throughput .throughput span").html(data["wan"]);
    $("#lan-card-throughput .throughput span").html(data["lan"]);
    $("#wanup_data").text(data["wan_tx_mbps"].toFixed(2));
    if (typeof window.wanup_chart != "undefined") {
      wanup_config.data.datasets[0].data.shift();
      wanup_config.data.datasets[0].data.push(data["wan_tx_mbps"]);
      window.wanup_chart.update();
      sessionStorage.setItem("wanup_data",JSON.stringify(wanup_config.data.datasets[0].data));
    }
    if (typeof window.wandn_chart != "undefined") {
      $("#wandn_data").text(data["wan_rx_mbps"].toFixed(2));
      wandn_config.data.datasets[0].data.shift();
      wandn_config.data.datasets[0].data.push(data["wan_rx_mbps"]);
      window.wandn_chart.update();
      sessionStorage.setItem("wandn_data",JSON.stringify(wandn_config.data.datasets[0].data));
    }
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(tpFuncID);}
  })
  .always(function() {
    waitingForThroughputResponse = false;
  });
}
$().ready(function(){
  setTimeout(updateBroadbandCard,0);
  if (window.autoRefreshEnabled) {
    bbFuncID=setInterval(updateBroadbandCard,15000);
    addRegisteredInterval(bbFuncID);
    var bbdiv = document.querySelector("#broadband-card-throughput");
    var bbhdr = document.querySelector("#broadband-card .header-title");
    bbhdr.parentNode.insertBefore(bbdiv,bbhdr.nextSibling);
    var landiv = bbdiv.cloneNode(true);
    var lanhdr = document.querySelector("#Local,#Local_Network_tab").closest(".header-title");
    landiv.setAttribute("id","lan-card-throughput");
    lanhdr.parentNode.insertBefore(landiv,lanhdr.nextSibling);
    if ($("div.throughput").is(":visible")) {
      updateThroughput();
      tpFuncID=setInterval(updateThroughput,2000);
      addRegisteredInterval(tpFuncID);
    }
  }
});
