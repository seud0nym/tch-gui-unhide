local readfile = require("web.content_helper").readfile
local floor,format,ipairs,match = math.floor,string.format,ipairs,string.match
local ngx = ngx
local TGU_CPU = ngx.shared.TGU_CPU

local M = {}

function M.getGatewayCardData()
  local cpu_usage = 0
  local prev_total = TGU_CPU:get("total") or 0
  local prev_idle = TGU_CPU:get("idle") or 0
  local user,sys,nice,idle,wait,irq,srq,zero
  local stat = io.open("/proc/stat")
  if stat then
    while (not user) do
      user,sys,nice,idle,wait,irq,srq,zero = match(stat:read("*l"),"cpu%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+).*")
    end
    stat:close()
  end
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

  local overlay = ""
  local df = io.popen("df -hP / | grep /$")
  if df then
    overlay = df:read()
    df:close()
  end

  local mem = {}
  local meminfo = io.popen("grep '^Mem' /proc/meminfo")
  if meminfo then
    for line in meminfo:lines() do
      local field, kB = match(line,"^Mem([^:]+):%s+(%d+)")
      mem[field] = tonumber(kB)
     end
    meminfo:close()
  end
  if not mem.Available then
    -- https://unix.stackexchange.com/a/261252
    meminfo = io.popen("awk -v low=$(grep low /proc/zoneinfo | awk '{k+=$2}END{print k;}') '{a[$1]=$2;} END{print a[\"MemFree:\"]+a[\"Active(file):\"]+a[\"Inactive(file):\"]+a[\"SReclaimable:\"]-(12*low);}' /proc/meminfo")
    if meminfo then
      mem.Available = meminfo:read("*n")
      meminfo:close()
    end
  end

  local disk_total,disk_free = match(overlay,"%S+%s+(%S+)%s+%S+%s+(%S+).*")
  local temp1_input = {}
  local temp_output = {}
  for i,f in ipairs(temp1_input) do
    temp_output[i] = readfile(f,"number",floor)/1000
  end

  return {
    cpu = cpu_usage,
    ram_free = mem.Free or 0,
    ram_avail = mem.Available or 0,
    ram_total = mem.Total or 0,
    disk_free = disk_free or "?",
    disk_total = disk_total or "?",
    uptime = tonumber(readfile("/proc/uptime","number",floor)),
    time = os.date("%d/%m/%Y %H:%M:%S",os.time()),
    load = readfile("/proc/loadavg","string"):sub(1,14),
    temps = table.concat(temp_output,"&deg; ").."&deg;",
  }
end

function M.secondsToTime(uptime,long)
  local d = floor(uptime/86400)
  local h = floor(uptime/3600)%24
  local m = floor(uptime/60)%60
  local s = floor(uptime%60)
  local dW = d == 1 and "day" or "days"
  local hW = long and (h == 1 and "hour" or "hours") or "hr"
  local mW = long and (m == 1 and "minute" or "minutes") or "min"
  local sW = long and (s == 1 and "second" or "seconds") or "sec"
  return format("%d %s, %d %s, %d %s and %d %s",d,dW,h,hW,m,mW,s,sW)
end

function M.getLandingPageData()
  local data = M.getGatewayCardData()
  return {
    cpu = format("%d%%",data.cpu),
    load = data.load,
    uptime = M.secondsToTime(data.uptime,true),
    ram_avail = format("%.2f%%",data.ram_avail/data.ram_total*100)
  }
end

return M
