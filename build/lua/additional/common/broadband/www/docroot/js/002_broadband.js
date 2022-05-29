var bbFuncID;
var tpFuncID;
function broadbandCardIcon(status){
  switch(status){
    case "up":
      return "\uf00c"; // Okay (Tick)
    case "disabled":
      return "\uf05e"; // Ban circle
    case "connecting":
      return "\uf110"; // Spinner
    default:
      return "\uf071"; // Warning Sign
  }
}
function updateBroadbandCard(){
  $.post("/ajax/broadband-status.lua",[tch.elementCSRFtoken()],function(data){
    $("#broadband-card-content").html(data["html"]);
    $("#broadband-card .content").removeClass("mirror").attr("data-bg-text",broadbandCardIcon(data["status"]));
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(bbFuncID);}
  });
}
function updateThroughput(){
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
  });
}
setTimeout(updateBroadbandCard,0);
$().ready(function(){
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
    updateThroughput();
    tpFuncID=setInterval(updateThroughput,2000);
    addRegisteredInterval(tpFuncID);
  }
});
