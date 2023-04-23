function showLoadingWrapper(){
  let busy_msg = $(".loading-wrapper");
  busy_msg.removeClass("hide");
  busy_msg[0].scrollIntoView();
  $(".modal-body").scrollLeft(0);
  return true;
}
$('div.modal-body').after('<div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>');
$("#save-config,.btn-table-edit,.btn-table-delete,.btn-table-modify,.btn-table-cancel,.btn-table-new,ul.nav li a").on("click",showLoadingWrapper);
