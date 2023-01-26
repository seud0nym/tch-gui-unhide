$('div.modal-body').after('<div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>');
$("#save-config").on("click",function(){
  let busy_msg = $(".loading-wrapper");
  busy_msg.removeClass("hide");
  busy_msg[0].scrollIntoView();
  $(".modal-body").scrollLeft(0);
  return true;
});
