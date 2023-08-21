var funcID;
var waitingForGatewayStatusResponse = false;
var updateGatewayCardSkipped = 3;
function secondsToTime(uptime) {
  let d = Math.floor(uptime / 86400);
  let h = Math.floor(uptime / 3600) % 24;
  let m = Math.floor(uptime / 60) % 60;
  let s = Math.floor(uptime % 60);
  let dDisplay = d > 0 ? d + " day" + (d == 1 ? " " : "s ") : "";
  let hDisplay = h > 0 ? h + " hr " : "";
  let mDisplay = m > 0 ? m + " min " : "";
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
    let ram_avail = Number(data["ram_avail"])
    let ram_free = Number(data["ram_free"]);
    let ram_total = Number(data["ram_total"]);
    let ram_used_pct = Number((ram_total-ram_free)/ram_total*100);
    $("#gateway-card-time").text(data["time"]);
    $("#gateway-card-uptime").text(secondsToTime(Number(data["uptime"])));
    $("#gateway-card-cpu").text(data["cpu"]);
    $("#gateway-card-load").text(data["load"]);
    $("#gateway-card-ram-avail").text((ram_avail/1024).toFixed(2));
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
      let ram_avail_pct = Number((ram_total-ram_avail)/ram_total*100);
      if ($("#ram_avail").is(":hidden")){
        $("#ram_data").text(ram_used_pct.toFixed(2));
      } else {
        $("#ram_data").text(ram_avail_pct.toFixed(2));
      }
      ram_config.data.datasets[0].data.shift();
      ram_config.data.datasets[0].data.push(ram_avail_pct);
      ram_config.data.datasets[1].data.shift();
      ram_config.data.datasets[1].data.push(ram_used_pct-ram_avail_pct);
      window.ram_chart.update();
      sessionStorage.setItem("ram_data",JSON.stringify({"available":ram_config.data.datasets[0].data,"used":ram_config.data.datasets[1].data}));
    }
  },"json")
  .fail(function(response){
    if(response.status==403||response.status==404){clearInterval(funcID);window.location.href="/gateway.lp?r="+Date.now();}
  })
  .always(function() {
    waitingForGatewayStatusResponse = false;
  });
}
$("#ram_free").click(function(){
  $(this).hide();
  $("#ram_avail").show();
  localStorage.setItem("ram_choice","#ram_avail");
})
$("#ram_avail").click(function(){
  $(this).hide();
  $("#ram_free").show();
  localStorage.setItem("ram_choice","#ram_free");
})
$().ready(function(){
  let ram_choice = localStorage.getItem("ram_choice");
  if (ram_choice == null) {
    ram_choice = "#ram_free";
    localStorage.setItem("ram_choice",ram_choice);
  }
  if (ram_choice == "#ram_avail") {
    $("#ram_free").hide();
    $("#ram_avail").show();
  }
  funcID=setInterval(updateGatewayCard,1000);
  addRegisteredInterval(funcID);
});
