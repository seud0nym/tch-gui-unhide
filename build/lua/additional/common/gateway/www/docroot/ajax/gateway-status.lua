local json = require("dkjson")
local proxy = require("datamodel")
local readfile = require("web.content_helper").readfile
local floor,ipairs,match = math.floor,ipairs,string.match
local ngx = ngx
local TGU_CPU = ngx.shared.TGU_CPU

local cpu_usage = 0
local prev_total = TGU_CPU:get("total") or 0
local prev_idle = TGU_CPU:get("idle") or 0
local user,sys,nice,idle,wait,irq,srq,zero
local stat = io.open("/proc/stat")
while (not user) do
  user,sys,nice,idle,wait,irq,srq,zero = match(stat:read("*line"),"cpu%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+).*")
end
stat:close()
if user then
  local ok,err
  local total = user + sys + nice + idle + wait + irq + srq + zero
  local diff_idle = idle - prev_idle
  local diff_total = total - prev_total
  cpu_usage = floor(((diff_total-diff_idle)/diff_total*100)+0.05)
  ok,err = TGU_CPU:safe_set("total",total)
  if not ok then
    ngx.log(ngx.ERR,"Failed to store current CPU total: ",err)
  end
  ok,err = TGU_CPU:safe_set("idle",idle)
  if not ok then
    ngx.log(ngx.ERR,"Failed to store current CPU idle: ",err)
  end
else
  ngx.log(ngx.ERR,"Failed to read CPU line from /proc/stat")
end

local df = io.popen("df -hP / | grep /$")
local overlay = df:read()
df:close()

local disk_total,disk_free = string.match(overlay,"%S+%s+(%S+)%s+%S+%s+(%S+).*")
local ram_total = tonumber(proxy.get("sys.mem.RAMTotal")[1].value or 0) or 0
local ram_free = tonumber(proxy.get("sys.mem.RAMFree")[1].value or 0) or 0
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
  temps = table.concat(temp_output,"&deg; ").."&deg;",
}

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
