--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local wifi_helper = require("ethernet-wifi_helper")

local find,match = string.find,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local gVIES = post_helper.getValidateInEnumSelect

-- The local interfaces an SSID can join (the interfaces listed on the Interfaces tab)
local wireless_radio = {}
for _,v in ipairs(proxy.getPN("rpc.wireless.ssid.",true)) do
  local ssid = match(v.path,"rpc%.wireless%.ssid%.@([^%.]+)%.")
  if ssid then
    wireless_radio[ssid] = true
  end
end
local lan_intfs = {}
local intfs = {} -- interface name -> { type, ifname }, as before any change
for _,v in ipairs(content_helper.convertResultToObject("rpc.network.interface.@.",proxy.get("rpc.network.interface."))) do
  if v.paramindex ~= "loopback" and v.type ~= "wan" and v.paramindex ~= "ppp" and v.paramindex ~= "ipoe" and v.paramindex ~= "wg0" and v.proto ~= "none" and v.proto ~= "wireguard" and (not find(v["ppp.ll_intf"],"wl") or wireless_radio[untaint(v["ppp.ll_intf"])]) then
    local name = untaint(v.paramindex)
    local cfg = proxy.get("uci.network.interface.@"..name..".type","uci.network.interface.@"..name..".ifname")
    if cfg then
      intfs[name] = { type = untaint(cfg[1].value), ifname = untaint(cfg[2].value) }
      lan_intfs[#lan_intfs+1] = { name, T((v.name and v.name ~= "") and v.name or name) }
    end
  end
end
table.sort(lan_intfs,function(a,b)
  if a[1] == "lan" then
    return true
  elseif b[1] == "lan" then
    return false
  end
  return a[1] < b[1]
end)

-- SSIDs that the GUI must not move (see ethernet-wifi_helper.classify)
local supported = wifi_helper.supported()
local class,ifaces = wifi_helper.classify(proxy,intfs)
local nets = {} -- wifi-iface section name -> network, as before any change
for iface,v in pairs(ifaces) do
  nets[iface] = untaint(v.network)
end

local wifi_columns = {
  { --[1]
    header = T"Name",
    name = "paramindex",
    param = "paramindex",
    type = "text",
    readonly = true,
  },
  { --[2]
    header = T"SSID",
    name = "ssid",
    param = "ssid",
    type = "text",
    readonly = true,
  },
  { --[3]
    header = T"Radio",
    name = "device",
    param = "device",
    type = "text",
    readonly = true,
  },
  { --[4]
    header = T"Interface",
    name = "network",
    param = "network",
    type = "select",
    values = lan_intfs,
    attr = { select = { class="span2" } },
  },
}

-- Refuse a move before anything is written: the table's per-row edit permission is the one stored when the page
-- was rendered, so an SSID that has become read-only since (EasyMesh enabled, say) must be checked again here
local validateNetworkChoice = gVIES(lan_intfs)
local function validateNetwork(value,object,key)
  local valid,helpmsg = validateNetworkChoice(value,object,key)
  if not valid then
    return valid,helpmsg
  end
  local allowed = ngx.ctx.session:retrieve("wifinetworks.allowedindexes") or {}
  local row = allowed[tonumber(object.index)]
  if not (supported and row and class[untaint(row.paramindex)] == "movable") then
    return nil,T"This Wi-Fi network cannot be moved."
  end
  return true
end

local wifi_valid = {
  network = validateNetwork,
}

local wifi_options = {
  tableid = "wifinetworks",
  basepath = "uci.wireless.wifi-iface.@.",
  canAdd = false,
  canDelete = false,
  canEdit = true,
  onModify = function(index,content)
    local ssid,new_net = untaint(index),untaint(content.network)
    if supported and class[ssid] == "movable" and nets[ssid] ~= new_net then
      for _,set in ipairs(wifi_helper.plan(ssid,new_net,intfs,nets)) do
        proxy.set(set[1],set[2])
      end
    end
  end
}

local function wifi_filter(data)
  local c = class[untaint(data.paramindex)]
  if not c then
    return false
  end
  return { canEdit = supported and c == "movable", canDelete = false }
end

local wifi_data,wifi_helpmsg = post_helper.handleTableQuery(wifi_columns,wifi_options,wifi_filter,nil,wifi_valid)

ngx.print(ui_helper.createHeader(T"Local Network",false,false),'\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-lan.lp")
ngx.print(
  ui_helper.createMessages(message_helper.popMessages()),'\
  <form class="form-horizontal" method="post" action="modals/ethernet-wifi-modal.lp">\
    <fieldset>\
      <legend>',T"Wi-Fi Networks",'</legend>',
        supported and ui_helper.createAlertBlock(T"Choose the interface each SSID belongs to. Firewall zone and DHCP follow the interface's settings; the SSID keeps its own AP isolation setting. SSIDs managed by EasyMesh cannot be moved.",{ alert = { class = "alert-info" } }) or ui_helper.createAlertBlock(T"Moving an SSID to another interface is not supported on this firmware.",{ alert = { class = "alert-warn" } }),
        ui_helper.createTable(wifi_columns,wifi_data,wifi_options,nil,wifi_helpmsg),'\
    </fieldset>\
  </form>\
  <script src="/js/modal-busy.js"></script>\
</div>\
',ui_helper.createFooter())
