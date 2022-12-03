--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
local dns_helper = require("dns_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local vSII = post_helper.validateStringIsIP
local vSIDN = post_helper.validateStringIsDomainName

local dnsmasq_path = dns_helper.dnsmasq_path

local rewrites_columns = {
  {
    header = T"Domain Address Query",
    name = "domain",
    type = "text",
    attr = { input = { class="span2" } },
  },
  {
    header = T"IPv4/IPv6 Address to Return",
    name = "value",
    param = "value",
    type = "text",
    attr = { input = { class="span2" } },
  },
}
local rewrites_valid = {
  domain = vSIDN,
  value = function(value,object)
    local ret,err = vSII(value)
    if ret then
      object.value = format("/%s/%s",object.domain,value)
      return true
    end
    return ret,err
  end
}
local rewrites_options = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add DNS rewrite",
  tableid = "rewrites",
  basepath = dnsmasq_path.."address.@.",
}
local function rewrites_filter(data)
  if data.value then
    local domain,ip = match(untaint(data.value),"/([^/]+)/(.+)")
    if domain and ip then
      return true
    end
  end
  return false
end
local rewrites_data,rewrites_helpmsg = dns_helper.handleDNSTableQuery(rewrites_columns,rewrites_options,rewrites_filter,nil,rewrites_valid)

ngx.print(ui_helper.createHeader(T"DNS",true,true),'\
<div class="modal-body update">\
'); lp.include("tabs-dns.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/dns-rewrites-modal.lp">\
    ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>',T"DNS Rewrites",'</legend>',
        ui_helper.createTable(rewrites_columns,rewrites_data,rewrites_options,nil,rewrites_helpmsg),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter())