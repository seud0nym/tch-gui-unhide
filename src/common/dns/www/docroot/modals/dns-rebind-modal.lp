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
local vB = post_helper.validateBoolean
local vSIDN = post_helper.validateStringIsDomainName

local dnsmasq_path = dns_helper.dnsmasq_path

local mapParams = {
  enabled = dnsmasq_path.."rebind_protection",
  localhost = dnsmasq_path.."rebind_localhost",
}
local mapValid = {
  enabled = vB,
  localhost = vB,
}
local content,content_helpmsg = post_helper.handleQuery(mapParams,mapValid)
if not content_helpmsg then
  content_helpmsg = {}
end

local domains_columns = {
  {
    header = T"Domain To Allow",
    name = "value",
    param = "value",
    type = "text",
    attr = { input = { class="span2" } },
  },
}
local domains_valid = {
  value = function(value,object)
  local ret,err = vSIDN(value)
  if ret then
    object.value = format("/%s/",object.value)
    return true
  end
  return ret,err
end
}
local domains_options = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add Domain",
  tableid = "rebind_domains",
  basepath = dnsmasq_path.."rebind_domain.@.",
}
local function domains_filter(data)
  if data.value then
    local domain = match(untaint(data.value),"/([^/]+)/")
    if domain then
      return true
    end
  end
  return false
end
local domains_data,domains_helpmsg = dns_helper.handleRebindTableQuery(domains_columns,domains_options,domains_filter,nil,domains_valid)

local switch_class_enable = {
  input = {
    class = "monitor-changes",
  }
}
local enabled_visible_attr = {
  group = {
    class = "monitor-enabled monitor-1",
  }
}

ngx.print(ui_helper.createHeader(T"DNS",true,true),'\
<div class="modal-body update">\
'); lp.include("tabs-dns.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/dns-rebind-modal.lp">\
    ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>',T"DNS Rebind Protection",'</legend>',
        ui_helper.createSwitch(T"Enabled","enabled",content["enabled"],switch_class_enable,content_helpmsg["enabled"]),
        ui_helper.createSwitch(T"Rebind localhost<span class='icon-question-sign' title='Allows upstream 127.0.0.0/8 responses (required for DNS based blacklist services).'></span>","localhost",content["localhost"],enabled_visible_attr,content_helpmsg["localhost"]),
        ui_helper.createTable(domains_columns,domains_data,domains_options,enabled_visible_attr,domains_helpmsg),'\
    </fieldset>\
    <script src="/js/modal-busy.js"></script>\
  </form>\
</div>\
',ui_helper.createFooter())