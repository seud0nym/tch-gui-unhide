local json = require("dkjson")
local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local mac_pattern = "[%x][%x]:[%x][%x]:[%x][%x]:[%x][%x]:[%x][%x]:[%x][%x]"
local name_pattern = mac_pattern.." (.*)"

local params = ngx.req.get_uri_args()
local mac = untaint(match(params.mac,mac_pattern))
if mac then
  local vendor

  local grep = io.popen(format("grep %s /tmp/mac.cache 2>/dev/null",mac))
  local result = grep:read("*a")
  grep:close()

  if result and result ~= "" then
    vendor = match(result,name_pattern)
  else
    local cmd = format("curl -fksm 2 https://api.maclookup.app/v2/macs/%s/company/name",mac)
    local curl = io.popen(cmd)
    result = curl:read("*a")
    curl:close()

    if result and result ~= "" then
      vendor = result
      local cache = io.open("/tmp/mac.cache","a")
      cache:write(mac," ",vendor,"\n")
      cache:close()
    end
  end

  if vendor then
    local data = {
      name = vendor,
    }

    local buffer = {}
    if json.encode (data,{ indent = false,buffer = buffer }) then
      ngx.say(buffer)
    else
      ngx.say("{}")
    end
    ngx.exit(ngx.HTTP_OK)
  else
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end
else
  ngx.exit(ngx.HTTP_BAD_REQUEST)
end
