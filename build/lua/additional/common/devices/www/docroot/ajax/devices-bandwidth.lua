local json = require("dkjson")
local dbh = require("devicesbandwidth_helper")

local labels,download,upload = dbh.getBandwithStatistics()

local data = {
  labels = labels,
  download = download,
  upload = upload,
}

local buffer = {}
if json.encode(data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
