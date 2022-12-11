local json = require("dkjson")
local proxy = require("datamodel")
local match = string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local params = ngx.req.get_uri_args()
local mac = untaint(match(params.mac,"[%x][%x]:[%x][%x]:[%x][%x]:[%x][%x]:[%x][%x]:[%x][%x]"))
if mac then
  proxy.set("rpc.gui.mac.find",mac)
  local vendor = proxy.get("rpc.gui.mac.vendor")

  if vendor then
    local data = {
      name = vendor[1].value,
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
