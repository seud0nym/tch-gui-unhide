local json = require("dkjson")
local content_helper = require("web.content_helper")
local socket = require("socket")

local tonumber = tonumber
local format = string.format
local TGU_MbPS = ngx.shared.TGU_MbPS

local wan_data = {
  lan_rx = "rpc.network.interface.@lan.rx_bytes",
  lan_tx = "rpc.network.interface.@lan.tx_bytes",
  wan_rx = "rpc.network.interface.@wan.rx_bytes",
  wan_tx = "rpc.network.interface.@wan.tx_bytes",
  wwan_rx = "rpc.network.interface.@wwan.rx_bytes",
  wwan_tx = "rpc.network.interface.@wwan.tx_bytes",
}
content_helper.getExactContent(wan_data)
for key,value in pairs(wan_data) do
  local ok,err
  local is = tonumber(value) or 0
  local now = socket.gettime()
  local from = TGU_MbPS:get(key.."_time")
  if from then
    local elapsed = now - from
    local was = TGU_MbPS:get(key.."_bytes")
    ok,err = TGU_MbPS:safe_set(key.."_mbps",(is - was) / elapsed * 0.000008)
    if not ok then
      ngx.log(ngx.ERR,"Failed to store current Mb/s into ",key,"_mbps: ",err)
    end
  end
  ok,err = TGU_MbPS:safe_set(key.."_time",now)
  if not ok then
    ngx.log(ngx.ERR,"Failed to store current time into ",key,"_time: ",err)
  end
  ok,err = TGU_MbPS:safe_set(key.."_bytes",is)
  if not ok then
    ngx.log(ngx.ERR,"Failed to store ",is," into ",key,"_bytes: ",err)
  end
end

local tx_mbps,rx_mbps = (TGU_MbPS:get("wan_tx_mbps") or 0) + (TGU_MbPS:get("wwan_tx_mbps") or 0),(TGU_MbPS:get("wan_rx_mbps") or 0) + (TGU_MbPS:get("wwan_rx_mbps") or 0)

local data = {
  wan = format("%.2f Mb/s <b>&uarr;</b><br>%.2f Mb/s <b>&darr;</b></span>",tx_mbps,rx_mbps),
  lan = format("%.2f Mb/s <b>&uarr;</b><br>%.2f Mb/s <b>&darr;</b></span>",TGU_MbPS:get("lan_tx_mbps") or 0,TGU_MbPS:get("lan_rx_mbps") or 0),
  wan_tx_mbps = tx_mbps,
  wan_rx_mbps = rx_mbps,
}

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
