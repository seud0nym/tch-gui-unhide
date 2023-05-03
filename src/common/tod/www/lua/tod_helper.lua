local M = {}
-- Localization
gettext.textdomain('webui-core')

local concat = table.concat
local ipairs = ipairs
local find = string.find
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

M.weekdays = {
  { "Mon",T"Mon." },
  { "Tue",T"Tue." },
  { "Wed",T"Wed." },
  { "Thu",T"Thu." },
  { "Fri",T"Fri." },
  { "Sat",T"Sat." },
  { "Sun",T"Sun." },
}

local validDays = {}
for _,v in ipairs(M.weekdays) do
  validDays[v[1]] = true
end

function M.daysOverlap(days1,days2)
  days1 = concat(days1," ")
  for _,day in ipairs(days2) do
    if find(days1,day,nil,true) then
      return true
    end
  end
  return nil
end

function M.validateScheduleOverlap(postcontent,v)
  if M.daysOverlap(postcontent.weekdays,v.days) and -- days overlap
    postcontent.start_time <= v.stop_time and
    postcontent.stop_time >= v.start_time then -- time overlaps
    return nil,T"Overlapping times are not allowed."
  end
  return true
end

function M.validateWeekdays(value,object)
  local validated
  if type(value) ~= "table" then
    if not value or value == "" then
      object.weekdays = {}
    else
      object.weekdays = {untaint(object.weekdays)}
    end
  else
    local days = {}
    for _,v in ipairs(value) do
      if v and v ~= "" then
        days[#days+1] = untaint(v)
      end
    end
    object.weekdays = days
  end
  if #object.weekdays == 0 then
    for _,v in ipairs(M.weekdays) do
      object.weekdays[#object.weekdays+1] = v[1]
    end
    validated = true
  end
  if not validated then
    for _,v in ipairs(object.weekdays) do
      local day = untaint(v)
      if not validDays[day] then
        return nil,"'"..day.."' is not a valid day"
      end
    end
  end
  return true
end

return M
