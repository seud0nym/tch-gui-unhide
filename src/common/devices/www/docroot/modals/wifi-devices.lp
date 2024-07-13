--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local devices_helper = require("devices_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local string,ngx = string,ngx
local match,format,gmatch,lower,sub = string.match,string.format,string.gmatch,string.lower,string.sub

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
    attr = { input = { class="span3" } },
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
local devices_column_index = {}
for k=1,#devices_columns do
  local v = devices_columns[k]
  devices_column_index[v.name] = k
end

local devices_options = {
  canEdit = false,
  canAdd = false,
  canDelete = false,
  tableid = "devices",
  basepath = "rpc.hosts.host.",
  sorted = devices_helper.sorter("FriendlyName",wifi),
  onModify = devices_helper.modify,
}

local devices_data,devices_helpmsg = post_helper.handleTableQuery(devices_columns,devices_options,devices_helper.filter(wifi,"Active",getargs.cache,false,true),nil,nil)
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
  row[devices_column_index["signalstrength"]] = signalstrength and signalstrength ~= "" and (tonumber(signalstrength)/2)-110 .. " dBm" or ""
  row[devices_column_index["linkrate"]] = linkrate and linkrate ~= "" and tonumber(linkrate)/1000 .. "Mbps" or ""
  local token1 = {}
  for token in gmatch(databytes,"[^/]+") do
    token1[#token1+1] = token
  end
  local tx_bytes = token1[1]
  local rx_bytes = token1[2]
  tx_bytes = tx_bytes and post_helper.convertData(tonumber(tx_bytes)) or ""
  rx_bytes = rx_bytes and post_helper.convertData(tonumber(rx_bytes)) or ""
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
