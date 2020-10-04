local json = require("dkjson")
local proxy = require("datamodel")
local readfile = require("web.content_helper").readfile
local ngx = ngx

local ram_total = tonumber(proxy.get("sys.mem.RAMTotal")[1].value or 0) or 0
local ram_free = tonumber(proxy.get("sys.mem.RAMFree")[1].value or 0) or 0
local cpu_usage = proxy.get("sys.proc.CPUUsage")[1].value or "0"

local data = {
  cpu = cpu_usage,
  ram_free = ram_free,
  ram_total = ram_total,
  uptime = readfile("/proc/uptime","number",floor),
  time = os.date("%d/%m/%Y %H:%M:%S",os.time()),
  load = readfile("/proc/loadavg","string"):sub(1,14),
}

local buffer = {}
if json.encode (data, { indent = false, buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
