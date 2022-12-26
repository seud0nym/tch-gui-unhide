local readfile = require("web.content_helper").readfile
local floor,ipairs,match = math.floor,ipairs,string.match
local ngx = ngx
local TGU_CPU = ngx.shared.TGU_CPU

local MemTotal

local function getMemInfo(field)
  local kB
  local meminfo = io.popen("grep '"..field..":' /proc/meminfo")
  if meminfo then
    kB = match(meminfo:read(),"[^:]+%s+(%d+).*")
    meminfo:close()
  end
  return tonumber(kB)
end

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

  if not MemTotal then
    MemTotal = getMemInfo("MemTotal")
  end

  local disk_total,disk_free = match(overlay,"%S+%s+(%S+)%s+%S+%s+(%S+).*")
  local temp1_input = {}
  local temp_output = {}
  for i,f in ipairs(temp1_input) do
    temp_output[i] = readfile(f,"number",floor)/1000
  end

  return {
    cpu = cpu_usage,
    ram_free = getMemInfo("MemFree") or 0,
    ram_total = MemTotal or 0,
    disk_free = disk_free or "?",
    disk_total = disk_total or "?",
    uptime = tonumber(readfile("/proc/uptime","number",floor)),
    time = os.date("%d/%m/%Y %H:%M:%S",os.time()),
    load = readfile("/proc/loadavg","string"):sub(1,14),
    temps = table.concat(temp_output,"&deg; ").."&deg;",
  }
end

function M.secondsToTime(uptime)
  local d = floor(uptime/86400)
  local h = floor(uptime/3600)%24
  local m = floor(uptime/60)%60
  local s = floor(uptime%60)
  local dDisplay = ""
  local hDisplay = ""
  local mDisplay = ""

  if d > 0 then
    dDisplay = d..N(" day "," days ",d)
  end
  if h > 0 then
    hDisplay = h.." hr "
  end
  if m > 0 then
    mDisplay = m.." min "
  end

  return dDisplay..hDisplay..mDisplay..s.." sec"
end

return M
