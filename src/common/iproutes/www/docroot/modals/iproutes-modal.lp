--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local match = string.match

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local network_rpc_content = content_helper.getMatchedContent("rpc.network.interface.")
local wan_intfs = {}
local wan6_intfs = {}
for _,v in ipairs (network_rpc_content) do
  local path = match(v.path,"rpc%.network%.interface%.@([^%.]+)%.")
  if v.type == "wan" or v.type == "lan" then
    if v.proto ~= "dhcpv6" and v.proto ~= "6rd" then
      wan_intfs[#wan_intfs+1] = { path,T(path) }
    else
      wan6_intfs[#wan6_intfs+1] = { path,T(path) }
    end
  end
end

local ipv4_dyn_route_columns = {
  {
    header = T"Destination",
    name = "Destination",
    param = "Destination",
    type = "text",
    readonly = true,
    attr = { input = { class="span2 centered" } },
  },
  {
    header = T"Mask",
    name = "Mask",
    param = "Mask",
    type = "text",
    readonly = true,
    attr = { input = { class="span2 centered" } },
  },
  {
    header = T"Gateway",
    name = "Gateway",
    param = "Gateway",
    type = "text",
    readonly = true,
    attr = { input = { class="span2 centered" } },
  },
  {
    header = T"Metric",
    name = "Metric",
    param = "Metric",
    type = "text",
    readonly = true,
    attr = { input = { class="span2 centered" } },
  },
  {
    header = T"Interface",
    name = "Iface",
    param = "Iface",
    type = "text",
    readonly = true,
    attr = { input = { class="span2 centered" } },
  },
  {
    header = T"Source",
    name = "Source",
    param = "Source",
    type = "text",
    readonly = true,
    attr = { input = { class="span2 centered" } },
  },
}
local ipv4_dyn_route_options = {
  canEdit = false,
  canAdd = false,
  canDelete = false,
  tableid = "ipv4dynamicroutes",
  basepath = "sys.class.iproutes.",
}
local ipv4_dyn_route_attributes = {
  group = {
    id = "ipv4dynamicroutescontainer"
  }
}
local ipv4_dyn_route_data,ipv4_dyn_route_helpmsg = content_helper.loadTableData("sys.class.iproutes.",ipv4_dyn_route_columns)

local function validateUniqueIPv4Route(object)
  for k = 1,#object do
    for n = k+1,#object do
      if object[k][1] == object[n][1] and object[k][2] == object[n][2] and object[k][3] == object[n][3] and object[k][4] == object[n][4] and object[k][5] == object[n][5] then
      return nil,{Iface = T"Duplicate Static Route Found"}
      end
    end
  end
  return true
end

local function validateIPv4GW(ipAddress,object)
  local gwIP = post_helper.validateStringIsIPv4(ipAddress)
  if gwIP then
    return post_helper.getAndValidation(post_helper.advancedIPValidation,post_helper.reservedIPValidation,post_helper.validateLXC)(ipAddress,object)
  end
  return nil,T"Invalid Gateway IP"
end

local ipv4_sta_route_columns = {
  {
    header = T"Destination",
    name = "Destination",
    param = "target",
    type = "text",
    attr = { input = { class="span2 centered no-save" } },
  },
  {
    header = T"Mask",
    name = "Mask",
    param = "netmask",
    type = "text",
    attr = { input = { class="span2 centered no-save" } },
  },
  {
    header = T"Gateway",
    name = "Gateway",
    param = "gateway",
    type = "text",
    attr = { input = { class="span2 centered no-save",id ="iproutesmodal_gateway" }},
  },
  {
    header = T"Metric",
    name = "Metric",
    param = "metric",
    type = "text",
    attr = { input = { class="span2 centered no-save" } },
  },
  {
    header = T"Interface",
    name = "Iface",
    param = "interface",
    type = "select",
    values = wan_intfs,
    attr = { select = { class="span2 centered no-save" } },
  },
}
local ipv4_sta_route_options = {
  tableid = "ipv4staticroutes",
  basepath = "uci.network.route.@.",
  createMsg = T"Add IPv4 Static Route",
  objectName = "ipv4_sta_route_"..post_helper.getRandomKey(),
  addNamedObject = true,
  valid = validateUniqueIPv4Route,
}
local ipv4_sta_route_valid = {
  Destination = post_helper.validateIPAndSubnet,
  Mask = post_helper.validateIPv4Netmask,
  Gateway = validateIPv4GW,
  Metric = post_helper.getValidateNumberInRange(-9999,9999),
  Interface = post_helper.getValidateInEnumSelect(wan_intfs),
}
local ipv4_sta_route_data,ipv4_sta_route_helpmsg = post_helper.handleTableQuery(ipv4_sta_route_columns,ipv4_sta_route_options,nil,nil,ipv4_sta_route_valid)

local lan_ipv6 = proxy.get("uci.network.interface.@lan.ipv6")[1].value == "1"
local ipv6_dyn_route_columns,ipv6_dyn_route_options,ipv6_dyn_route_attributes,ipv6_dyn_route_data,ipv6_dyn_route_helpmsg
local ipv6_sta_route_columns,ipv6_sta_route_options,ipv6_sta_route_valid,ipv6_sta_route_data,ipv6_sta_route_helpmsg
if lan_ipv6 then
  ipv6_dyn_route_columns = {
    {
      header = T"Destination",
      name = "Destination",
      param = "Destination",
      type = "text",
      readonly = true,
      attr = { input = { class="span2 centered" } },
    },
    {
      header = T"Gateway",
      name = "Gateway",
      param = "Gateway",
      type = "text",
      readonly = true,
      attr = { input = { class="span2 centered" } },
    },
    {
      header = T"Metric",
      name = "Metric",
      param = "Metric",
      type = "text",
      readonly = true,
      attr = { input = { class="span2 centered" } },
    },
    {
      header = T"Interface",
      name = "Iface",
      param = "Iface",
      type = "text",
      readonly = true,
      attr = { input = { class="span2 centered" } },
    },
  }
  ipv6_dyn_route_options = {
    canEdit = false,
    canAdd = false,
    canDelete = false,
    tableid = "ipv6dynamicroutes",
    basepath = "sys.class.ip6routes.",
  }
  ipv6_dyn_route_attributes = {
    group = {
      id = "ipv6dynamicroutescontainer"
    }
  }
  ipv6_dyn_route_data,ipv6_dyn_route_helpmsg = content_helper.loadTableData("sys.class.ip6routes.",ipv6_dyn_route_columns)

  local function validateUniqueIPv6Route(object)
    for k = 1,#object do
      for n = k+1,#object do
        if object[k][1] == object[n][1] and object[k][2] == object[n][2] and object[k][3] == object[n][3] and object[k][4] == object[n][4] then
        return nil,{Iface = T"Duplicate Static Route Found"}
        end
      end
    end
    return true
  end

  local function validateIPv6GW(ipAddress,object)
    local gwIP = post_helper.validateStringIsIPv6(ipAddress)
    if gwIP then
      return post_helper.getAndValidation(post_helper.advancedIPValidation,post_helper.reservedIPValidation,post_helper.validateLXC)(ipAddress,object)
    end
    return nil,T"Invalid Gateway IP"
  end

  ipv6_sta_route_columns = {
    {
      header = T"Destination",
      name = "Destination",
      param = "target",
      type = "text",
      attr = { input = { class="span2 centered no-save" } },
    },
    {
      header = T"Gateway",
      name = "Gateway",
      param = "gateway",
      type = "text",
      attr = { input = { class="span2 centered no-save",id ="iproutesmodal_gateway" }},
    },
    {
      header = T"Metric",
      name = "Metric",
      param = "metric",
      type = "text",
      attr = { input = { class="span2 centered no-save" } },
    },
    {
      header = T"Interface",
      name = "Iface",
      param = "interface",
      type = "select",
      values = wan6_intfs,
      attr = { select = { class="span2 centered no-save" } },
    },
  }
  ipv6_sta_route_options = {
    tableid = "ipv6staticroutes",
    basepath = "uci.network.route6.@.",
    createMsg = T"Add IPv6 Static Route",
    objectName = "ipv6_sta_route_"..post_helper.getRandomKey(),
    addNamedObject = true,
    valid = validateUniqueIPv6Route,
  }
  ipv6_sta_route_valid = {
    Destination = post_helper.validateIPAndSubnet,
    Mask = post_helper.validateipv6Netmask,
    Gateway = validateIPv6GW,
    Metric = post_helper.getValidateNumberInRange(-9999,9999),
    Interface = post_helper.getValidateInEnumSelect(wan_intfs),
  }
  ipv6_sta_route_data,ipv6_sta_route_helpmsg = post_helper.handleTableQuery(ipv6_sta_route_columns,ipv6_sta_route_options,nil,nil,ipv6_sta_route_valid)
end

ngx.print(ui_helper.createHeader(T"IP Routing",false,true),'\
<div class="modal-body update">\
'); lp.include("tabs-internet.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/iproutes-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>',T"Dynamic Routes",'</legend>\
      <legend><small>',T"IPv4",'</small></legend>',
      ui_helper.createTable(ipv4_dyn_route_columns,ipv4_dyn_route_data,ipv4_dyn_route_options,ipv4_dyn_route_attributes,ipv4_dyn_route_helpmsg),'')
      if lan_ipv6 then
        ngx.print('\
        <legend><small>',T"IPv6",'</small></legend>',
        ui_helper.createTable(ipv6_dyn_route_columns,ipv6_dyn_route_data,ipv6_dyn_route_options,ipv6_dyn_route_attributes,ipv6_dyn_route_helpmsg),'')
      end
      ngx.print('\
    </fieldset>\
    <fieldset>\
      <legend>',T"Static Routes",'</legend>\
      <legend><small>',T"IPv4",'</small></legend>',
      ui_helper.createTable(ipv4_sta_route_columns,ipv4_sta_route_data,ipv4_sta_route_options,nil,ipv4_sta_route_helpmsg),'')
      if lan_ipv6 then
        ngx.print('\
        <legend><small>',T"IPv6",'</small></legend>',
        ui_helper.createTable(ipv6_sta_route_columns,ipv6_sta_route_data,ipv6_sta_route_options,nil,ipv6_sta_route_helpmsg),'')
      end
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
'); ngx.print(ui_helper.createFooter()); ngx.print('\
<script>\
setTimeout(function() {\
  $("#dynamicroutescontainer").load("modals/iproutes-modal.lp #dynamicroutes");\
},250);\
</script>\
');