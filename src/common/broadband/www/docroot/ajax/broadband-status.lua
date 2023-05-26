local json = require("dkjson")
local content_helper = require("web.content_helper")
local bbch = require("broadbandcard_helper")

local wan_data = {
  wans_enable = "uci.wansensing.global.enable",
}
content_helper.getExactContent(wan_data)

local html,status
xpcall(
  function()
    html,status = bbch.getBroadbandCardHTML(wan_data.wans_enable)
  end,
  function(err)
    html,status = {"<pre>ERROR: ",err,"</pre>"},"unknown"
    ngx.log(ngx.ERR, debug.traceback(err))
 end
)

local data = {
  html = table.concat(html,"\n"),
  status = status,
}

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
