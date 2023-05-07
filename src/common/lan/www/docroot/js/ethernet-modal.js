function rand(max){
  return Math.floor(Math.random()*max);
}
function rand16() {
  return rand(2**16).toString(16);
}
 $(function() {
    var gen_ula_span = document.createElement("SPAN");
    gen_ula_span.setAttribute("id","random_ula_prefix");
    gen_ula_span.setAttribute("class","btn icon-random");
    gen_ula_span.setAttribute("style","padding:5px 3px 8px 3px;");
    gen_ula_span.setAttribute("title","Click to generate a random ULA prefix");
    $("#ula_prefix").after(gen_ula_span);
    $("#random_ula_prefix").click(function(){
      var i=$("#ula_prefix");
      i.val((parseInt("fd00",16)+rand(2**8)).toString(16)+":"+rand16()+":"+rand16()+"::/48");
      var e=jQuery.Event("keydown");
      e.which=e.keyCode=13;
      i.trigger(e);
    });
    var iPv6StateOnlyChanged = 0;
    $("input,select").on("change",function(){
       if(this.id == "localIPv6" && iPv6StateOnlyChanged == 0)
         iPv6StateOnlyChanged = 1;
       else
         iPv6StateOnlyChanged = 2;
    });
    //Override the save button click event to update the IPv6 state alone.
    $("#save-config").click(function(){
         if(iPv6StateOnlyChanged == 1){
          var params = [];
          params.push({
            name : "action",
            value : "SAVE"
          },
          {
            name : "iPv6StateOnlyChanged",
            value : "yes"
          },
          {
            name : "localIPv6",
            value : $("#localIPv6").val()
          },tch.elementCSRFtoken());
          var target = $(".modal form").attr("action");
          $.post(target,params,function(response){
            //The following block of code used to display the success/error message and manage the footer.
            $(".alert").remove();
            $("form").prepend(response);
            $("#modal-changes").attr("style","display:none");
            $("#modal-no-change").attr("style","display:block");
            iPv6StateOnlyChanged = 0;
          });
          return false;
         }
     });
});
$("[name='sleases_mac'],[name='dns_v4_pri'],[name='ipv4_dns_sec'],[name='dns_v6_pri'],[name='dns_v6_sec'],[name='tags_dns1'],[name='tags_dns2']").change(function () {
  if ((this.value) == "custom") {
    $(this).replaceWith($('<input/>',{'type':'text','name':this.name,'class':this.className}));
  }
});
$("#sleases>tbody>tr:not(.line-edit)>td:nth-child(1)").each(function(){
  $(this).text(function(i,content){
    return content.split(" ")[0];
  });
});
$('input[name="localdevIP"]').keydown(function(){
  var msg = $("#lanipchange-msg");
  var msg_dst = $(this);
  msg_dst.after(msg);
  msg.removeClass("hide");
});
$('.modal input[name="lanport"]').click(function(){
  $("#modal-no-change").fadeOut(300);
  $("#modal-changes").delay(350).fadeIn(300);
});
$('[id^="remove-interface-"]').click(function(evt){
  var id = evt.currentTarget.id.split("-")[2]
  if (confirm("Are you really, absolutely sure you want to remove the '"+id+"' interface?\n\nThis cannot be undone!")) {
    showLoadingWrapper();
    var t = $(".modal form"),e = t.serializeArray();
    e.push({
      name: "del_intf",
      value: id
    },{
      name: "CSRFtoken",
      value: $("meta[name=CSRFtoken]").attr("content")
    });
    tch.loadModal(t.attr("action"),e)
  }
  evt.preventDefault()
  return false;
});
