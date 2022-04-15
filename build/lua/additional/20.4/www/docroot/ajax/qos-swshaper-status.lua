local json = require("dkjson")
local proxy = require("datamodel")

local shape_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-swshaper-modal.lp" data-id="qos-modal"'

local count = 0
local swshapers = proxy.getPN("Device.QoS.Shaper.",true)
for _,v in ipairs(swshapers) do
  local enabled = proxy.get(v.path.."Enable")
  if enabled and enabled[1].value == "true" then
    count = count + 1
  end
end

local data = {
  html = string.format(N("<strong %1$s>%2$d Shaper</strong>","<strong %1$s>%2$d Shapers</strong> enabled",count),shape_link,count),
}

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
