--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local common = require("common_helper")
local devices_helper = require("devices_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local string,ngx = string,ngx
local match,format,gmatch,lower,sub = string.match,string.format,string.gmatch,string.lower,string.sub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

--region get args
local getargs = ngx.req.get_uri_args()
local tabs = "tabs-wifi-booster.lp"
local header = T"Wi-Fi Booster"
local modal_param=""
if getargs.dev and getargs.dev == "1" then
  modal_param = "?dev=1"
  tabs = "tabs-devices.lp"
  header = T"Devices"
end
--endregion

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local wifi = devices_helper.getWiFi()

-- !!! These columns are styled in common-override.css, and the indexes are referenced below !!!
local devices_columns = {
  {--[1]
    header = T"Status",
    name = "state",
    param = "State",
    type = "light",
    readonly = true,
  },
  {--[2]
    header = T"Hostname",
    name = "FriendlyName",
    param = "FriendlyName",
    type = "text",
    attr = { input = { class="span3 no-save" } },
  },
  {--[3]
    header = T"IPv4",
    name = "ipv4",
    param = "IPv4",
    type = "text",
    readonly = true,
  },
  {--[4]
    header = T"IPv6",
    name = "ipv6",
    param = "IPv6",
    type = "text",
    readonly = true,
  },
  {--[5]
    header = T"MAC",
    name = "macaddress",
    param = "MACAddress",
    type = "text",
    readonly = true,
    additional_class = 'data-type="mac"',
  },
  {--[6]
    header = T"Connection Type",
    name = "interfacetype",
    param = "InterfaceType",
    type = "text",
    readonly = true,
  },
  {--[7]
    header = T"Signal Strength",
    name = "signalstrength",
    param = "Rssi",
    readonly = true,
  },
  {--[8]
    header = T"Link Rate",
    name = "linkrate",
    param = "MaxPhyRate",
    type = "text",
    readonly = true,
  },
  { --[9]
    header = T"Tx/Rx Bytes",
    name = "databytes",
    param = "TxRxBytes",
    type = "text",
    readonly = true,
  },
  {--[11]
    header = T"",
    name = "IPAddress",
    param = "IPAddress",
    type = "hidden",
    readonly = true,
  },
}

-- post_helper.convertData is not available before 20.4
local convertData = post_helper.convertData or function(bytes)
  if bytes then
    local _,text = common.bytes2string(bytes)
    return text
  end
end

local devices_column_index = {}
for k=1,#devices_columns do
  local v = devices_columns[k]
  devices_column_index[v.name] = k
end

local devices_options = {
  canEdit = true,
  canAdd = false,
  canDelete = false,
  tableid = "devices",
  basepath = "rpc.hosts.host.",
  sorted = devices_helper.sorter("FriendlyName",wifi),
  onModify = devices_helper.modify,
}

local devices_data,devices_helpmsg = post_helper.handleTableQuery(devices_columns,devices_options,devices_helper.filter(wifi,"Active",getargs.cache,false,true),nil,nil)

-- Firmware without the Rssi, MaxPhyRate and TxRxBytes host parameters reports them per access point station.
-- They are only read when a host is missing one of them.
local stations
local function getStations()
  if not stations then
    stations = {}
    local aps = proxy.getPN("rpc.wireless.ap.",true) or {}
    for k=1,#aps do
      local sta_paths = proxy.getPN(aps[k].path.."station.",true) or {}
      for s=1,#sta_paths do
        local path = sta_paths[s].path
        local values = proxy.get(path.."state",path.."rssi",path.."tx_phy_rate",path.."tx_bytes",path.."rx_bytes")
        if values then
          local sta = {}
          for v=1,#values do
            sta[values[v].param] = untaint(values[v].value)
          end
          if sta.state ~= "Disconnected" and sta.state ~= "Authenticated" then
            stations[lower(untaint(match(path,"@([^@%.]+)%.$")))] = sta
          end
        end
      end
    end
  end
  return stations
end

local group_data = {}
group_data[wifi.multiap and devices_helper.hostname or "Wireless"] = {}
for i=1,#devices_data do
  local row = devices_data[i]
  local iface = match(row[devices_column_index["interfacetype"]],"(.+) %- [%d%.]+GHz")
  if iface then
    row[devices_column_index["interfacetype"]] = sub(row[devices_column_index["interfacetype"]],#iface+3)
  else
    iface = row[devices_column_index["interfacetype"]]
  end
  if not group_data[iface] then
    group_data[iface] = {}
  end
  local signalstrength = row[devices_column_index["signalstrength"]]
  local linkrate = row[devices_column_index["linkrate"]]
  local databytes = row[devices_column_index["databytes"]]
  local sta
  if not signalstrength or signalstrength == "" or not linkrate or linkrate == "" or not databytes or databytes == "" then
    sta = getStations()[lower(match(untaint(row[devices_column_index["macaddress"]]),"%x%x:%x%x:%x%x:%x%x:%x%x:%x%x") or "")]
  end
  if sta and (not linkrate or linkrate == "") then
    linkrate = sta.tx_phy_rate
  end
  if sta and (not databytes or databytes == "") then
    databytes = format("%s/%s",sta.tx_bytes,sta.rx_bytes)
  end
  row[devices_column_index["signalstrength"]] = signalstrength and signalstrength ~= "" and (tonumber(signalstrength)/2)-110 .. " dBm" or sta and sta.rssi and sta.rssi ~= "" and sta.rssi.." dBm" or ""
  row[devices_column_index["linkrate"]] = linkrate and linkrate ~= "" and tonumber(linkrate)/1000 .. "Mbps" or ""
  local token1 = {}
  for token in gmatch(databytes,"[^/]+") do
    token1[#token1+1] = token
  end
  local tx_bytes = token1[1]
  local rx_bytes = token1[2]
  tx_bytes = tx_bytes and convertData(tonumber(tx_bytes)) or ""
  rx_bytes = rx_bytes and convertData(tonumber(rx_bytes)) or ""
  if tx_bytes ~= "" and rx_bytes ~= "" then
    row[devices_column_index["databytes"]] = format("%s / %s ",tx_bytes,rx_bytes)
  else
    row[devices_column_index["databytes"]] = ""
  end
  group_data[iface][#group_data[iface]+1] = row
end

local group = {}
for k in pairs(group_data) do
  group[#group+1] = k
end
table.sort(group, function (a, b)
  return lower(a or "") < lower(b or "")
end)

ngx.print(ui_helper.createHeader(header,false,true,10),'\
<div class="modal-body update">\
'); lp.include(tabs); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/wifi-devices.lp',modal_param,'">',
    ui_helper.createMessages(message_helper.popMessages()))
    for k=1,#group do
      local g = group[k]
      local data = group_data[g]
      ngx.print('\
      <span id="',T(g),'"><b>',T(g),'</b><br/>'..T'Connected Devices:'..'',#data,'<br /></span>\
      <fieldset>',
        ui_helper.createTable(devices_columns,data,devices_options,nil,devices_helpmsg),'\
      </fieldset>')
    end
    ngx.print('\
  </form>\
</div>\
<script src="/js/modal-busy.js"></script>\
',ui_helper.createFooter(),'\
<script src="/js/device-modal.js"></script>\
');
