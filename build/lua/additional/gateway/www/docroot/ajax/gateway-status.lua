local json = require("dkjson")
local proxy = require("datamodel")
local readfile = require("web.content_helper").readfile
local floor, ipairs = math.floor, ipairs
local ngx = ngx

local cmd = io.popen("df -hP / | grep /$")
local overlay = cmd:read()
cmd:close()

local disk_total, disk_free = string.match(overlay, "%S+%s+(%S+)%s+%S+%s+(%S+).*")
local ram_total = tonumber(proxy.get("sys.mem.RAMTotal")[1].value or 0) or 0
local ram_free = tonumber(proxy.get("sys.mem.RAMFree")[1].value or 0) or 0
local cpu_usage = proxy.get("sys.proc.CPUUsage")[1].value or "0"
local temp1_input = {}
local temp_output = {}
for i,f in ipairs(temp1_input) do
  temp_output[i] = readfile(f,"number",floor) / 1000
end

local data = {
  cpu = cpu_usage,
  ram_free = ram_free,
  ram_total = ram_total,
  disk_free = disk_free,
  disk_total = disk_total,
  uptime = readfile("/proc/uptime","number",floor),
  time = os.date("%d/%m/%Y %H:%M:%S",os.time()),
  load = readfile("/proc/loadavg","string"):sub(1,14),
  temps = table.concat(temp_output, "&deg; ") .. "&deg;",
}

local buffer = {}
if json.encode (data, { indent = false, buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
