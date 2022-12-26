var funcID;
var waitingForGatewayStatusResponse = false;
var updateGatewayCardSkipped = 3
function secondsToTime(uptime) {
  var d = Math.floor(uptime / 86400);
  var h = Math.floor(uptime / 3600) % 24;
  var m = Math.floor(uptime / 60) % 60;
  var s = Math.floor(uptime % 60);
  var dDisplay = d > 0 ? d + " day" + (d == 1 ? " " : "s ") : "";
  var hDisplay = h > 0 ? h + " hr " : "";
  var mDisplay = m > 0 ? m + " min " : "";
  return dDisplay + hDisplay + mDisplay + s + " sec";
}
function updateGatewayCard(){
  if((updateGatewayCardSkipped < 5 && window.activeXHR.length > 2) || waitingForGatewayStatusResponse){
    updateGatewayCardSkipped ++;
    return;
  }
  waitingForGatewayStatusResponse = true;
  updateGatewayCardSkipped = 0;
  $.post("/ajax/gateway-status.lua",[tch.elementCSRFtoken()],function(data){
    var ram_free = Number(data["ram_free"]);
    var ram_total = Number(data["ram_total"]);
    var ram_used_pct = Number((ram_total-ram_free)/ram_total*100);
    $("#gateway-card-time").text(data["time"]);
    $("#gateway-card-uptime").text(secondsToTime(Number(data["uptime"])));
    $("#gateway-card-cpu").text(data["cpu"]);
    $("#gateway-card-load").text(data["load"]);
    $("#gateway-card-ram-free").text((ram_free/1024).toFixed(2));
    $("#gateway-card-ram-total").text((ram_total/1024).toFixed(2));
    $("#gateway-card-disk-free").text(data["disk_free"]);
    $("#gateway-card-disk-total").text(data["disk_total"]);
    $("#gateway-card-temps").html(data["temps"]);
    if (typeof window.cpu_chart != "undefined") {
      $("#cpu_data").text(data["cpu"]);
      cpu_config.data.datasets[0].data.shift();
      cpu_config.data.datasets[0].data.push(data["cpu"]);
      window.cpu_chart.update();
      sessionStorage.setItem("cpu_data",JSON.stringify(cpu_config.data.datasets[0].data));
    }
    if (typeof window.ram_chart != "undefined") {
      $("#ram_data").text(ram_used_pct.toFixed(2));
      ram_config.data.datasets[0].data.shift();
      ram_config.data.datasets[0].data.push(ram_used_pct);
      window.ram_chart.update();
      sessionStorage.setItem("ram_data",JSON.stringify(ram_config.data.datasets[0].data));
    }
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(funcID);window.location.href="/gateway.lp?r="+Date.now();}
  })
  .always(function() {
    waitingForGatewayStatusResponse = false;
  });
}
$().ready(function(){
  funcID=setInterval(updateGatewayCard,1000);
  addRegisteredInterval(funcID);
});
