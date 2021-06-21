local json = require("dkjson")
local bbch = require("broadbandcard_helper")

local wanHTML, lanHTML = bbch.getThroughputHTML()

local data = {
  wan = wanHTML,
  lan = lanHTML,
}

local buffer = {}
if json.encode (data, { indent = false, buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
