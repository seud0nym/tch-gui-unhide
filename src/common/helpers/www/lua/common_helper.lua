local floor,format = math.floor,string.format
local tonumber = tonumber

local M = {}

function M.bytes2string(s_bytes)
  if s_bytes=="" then
    return "0<small>B</small>"
  else
    local bytes = tonumber(s_bytes)
    local kb = bytes/1024
    local mb = kb/1024
    local gb = mb/1024
    if gb >= 1 then
      return format("%.1f<small>GB</small>",gb),format("%.1fGB",gb)
    elseif mb >= 1 then
      return format("%.1f<small>MB</small>",mb),format("%.1fMB",mb)
    elseif kb >= 1 then
      return format("%.1f<small>KB</small>",kb),format("%.1fKB",kb)
    else
      return format("%d<small>B</small>",bytes),format("%dB",bytes)
    end
  end
end

function M.secondsToTime(time)
  local time_no = tonumber(time)
  if (time_no and time_no >= 0) then
    return format("%dd %02d:%02d:%02d",floor(time_no/86400),floor(time_no/3600)%24,floor(time_no/60)%60,floor(time_no)%60)
  end
  return nil,T"Positive number expected."
end

return M
