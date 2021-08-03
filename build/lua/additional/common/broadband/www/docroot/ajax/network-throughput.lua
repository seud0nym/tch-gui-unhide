local json = require("dkjson")
local bbch = require("broadbandcard_helper")

local wanHTML, lanHTML, wan_tx_mbps, wan_rx_mbps = bbch.getThroughputHTML()

local data = {
  wan = wanHTML,
  lan = lanHTML,
  wan_tx_mbps = wan_tx_mbps, 
  wan_rx_mbps = wan_rx_mbps,
}

local buffer = {}
if json.encode (data, { indent = false, buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
