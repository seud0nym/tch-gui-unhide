--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local fw_tables = {
  { "filter",T"Filters"},
  { "nat",T"NAT (IPv4 ONLY)"},
  { "mangle",T"Mangle"},
  { "raw",T"Raw"},
}
local fw_table_name = "filter"

local iptables = {
  filter = proxy.get("rpc.gui.firewall.iptables_filter."),
  nat = proxy.get("rpc.gui.firewall.iptables_nat."),
  mangle = proxy.get("rpc.gui.firewall.iptables_mangle."),
  raw = proxy.get("rpc.gui.firewall.iptables_raw."),
}
local ip6tables = {
  filter = proxy.get("rpc.gui.firewall.ip6tables_filter."),
  mangle = proxy.get("rpc.gui.firewall.ip6tables_mangle."),
  raw = proxy.get("rpc.gui.firewall.ip6tables_raw."),
}

local fw_table_attributes = {
  select = {
      class ="monitor-changes no-save",
  },
}

ngx.print(ui_helper.createHeader(T"Firewall",true,true),'\
<div class="modal-body update" id="fw-iptables">\
'); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-iptables-modal.lp">',
    ui_helper.createInputSelect(T"Table","fw_table",fw_tables,fw_table_name,fw_table_attributes),'\
    <div class="control-group">\
      <label class="control-label">Filter</label>\
      <div class="controls">\
        <input data-type="search" name="Filter" class="edit-input span3 no-save" placeholder="Search Rules..." id="searchInput" type="text">\
      </div>\
    </div>\
    <div class="control-group">\
      <label class="control-label">Show</label>\
      <div class="controls">\
        <label class="checkbox inline"><input type="checkbox" id="showIPv4" checked="" class="no-save"> IPv4</label>\
        <label class="checkbox inline"><input type="checkbox" id="showIPv6" checked="" class="no-save"> IPv6</label>\
      </div>\
    </div>\
  </form>\
  <fieldset id="fw-iptables-filter" class="monitor-fw_table monitor-filter monitor-1">\
    <legend>',T"IPv4 Packet Filter Rules",'</legend>\
      <table id="iptables-filter" class="table table-striped">')
        for _,v in pairs(iptables["filter"]) do
          ngx.print(untaint(v.value))
        end
      ngx.print('</table>\
  </fieldset>\
  <fieldset id="fw-ip6tables-filter" class="monitor-fw_table monitor-filter monitor-1">\
    <legend>',T"IPv6 Packet Filter Rules",'</legend>\
      <table id="ip6tables-filter" class="table table-striped">')
        for _,v in pairs(ip6tables["filter"]) do
          ngx.print(untaint(v.value))
        end
      ngx.print('</table>\
  </fieldset>\
  <fieldset id="fw-iptables-nat" class="monitor-fw_table monitor-nat monitor-1">\
    <legend>',T"IPv4 Packet NAT Rules",'</legend>\
      <table id="iptables-nat" class="table table-striped">')
        for _,v in pairs(iptables["nat"]) do
          ngx.print(untaint(v.value))
        end
      ngx.print('</table>\
  </fieldset>\
  <fieldset id="fw-iptables-mangle" class="monitor-fw_table monitor-mangle monitor-1">\
    <legend>',T"IPv4 Packet Mangle Rules",'</legend>\
      <table id="iptables-mangle" class="table table-striped">')
        for _,v in pairs(iptables["mangle"]) do
          ngx.print(untaint(v.value))
        end
      ngx.print('</table>\
  </fieldset>\
  <fieldset id="fw-ip6tables-mangle" class="monitor-fw_table monitor-mangle monitor-1">\
    <legend>',T"IPv6 Packet Mangle Rules",'</legend>\
      <table id="ip6tables-mangle" class="table table-striped">')
        for _,v in pairs(ip6tables["mangle"]) do
          ngx.print(untaint(v.value))
        end
      ngx.print('</table>\
  </fieldset>\
  <fieldset id="fw-iptables-raw" class="monitor-fw_table monitor-raw monitor-1">\
    <legend>',T"IPv4 Packet Raw Rules",'</legend>\
      <table id="iptables-raw" class="table table-striped">')
        for _,v in pairs(iptables["raw"]) do
          ngx.print(untaint(v.value))
        end
      ngx.print('</table>\
  </fieldset>\
  <fieldset id="fw-ip6tables-raw" class="monitor-fw_table monitor-raw monitor-1">\
    <legend>',T"IPv6 Packet Raw Rules",'</legend>\
      <table id="ip6tables-raw" class="table table-striped">')
        for _,v in pairs(ip6tables["raw"]) do
          ngx.print(untaint(v.value))
        end
      ngx.print('</table>\
  </fieldset>\
</div>\
',ui_helper.createFooter(),'\
<script src="/js/firewall-iptables-modal.js"></script>\
');
