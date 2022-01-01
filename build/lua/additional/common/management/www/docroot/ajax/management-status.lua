local json = require("dkjson")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local theme_helper = require("theme-schedule_helper")
local format = string.format

local counts = {
  cron = "rpc.gui.cron.ActiveNumberOfEntries",
  init = "rpc.gui.init.InitNumberOfEntries",
  proc = "rpc.gui.proc.ProcNumberOfEntries",
}
content_helper.getExactContent(counts)

local function toTime(hh,mm)
  local m
  if hh >= 12 then
    hh = hh - 12
    m = "p"
  else
    m = "a"
  end
  return format("%d:%02d%sm",hh,mm,m)
end

local light,night = theme_helper.getThemeSchedule()
local on_schedule,theme_status = "0","disabled"
if light.hh and light.mm and night.hh and night.mm and light.enabled and night.enabled then
  on_schedule = "1"
  local now = tonumber(os.date("%H")) * 60 + tonumber(os.date("%M"))
  local light_time = light.hh * 60 + light.mm
  local night_time = night.hh * 60 + night.mm
  if light_time <= night_time then
    if now >= light_time and now <= night_time then
      theme_status = toTime(night.hh,night.mm)
    else
      theme_status = toTime(light.hh,light.mm)
    end
  else
    if now >= night_time and now <= light_time then
      theme_status = toTime(light.hh,light.mm)
    else
      theme_status = toTime(night.hh,night.mm)
    end
  end
end

local html = {}

html[#html+1] = ui_helper.createSimpleLight(on_schedule,'<span class="modal-link" data-toggle="modal" data-remote="/modals/theme-modal.lp" data-id="theme-modal">Scheduled Theme Change</span> '..theme_status)
html[#html+1] = '<span class="simple-desc">'
html[#html+1] = '<i class="icon-cog status-icon"></i>'
html[#html+1] = format(N("<strong %s>%d process</strong> running","<strong %s>%d processes</strong> running",counts.proc),'class="modal-link" data-toggle="modal" data-remote="/modals/system-proc-modal.lp" data-id="system-proc-modal"',counts.proc)
html[#html+1] = '</span>'
html[#html+1] = '<span class="simple-desc">'
html[#html+1] = '<i class="icon-calendar status-icon"></i>'
html[#html+1] = format(N("<strong %s>%d scheduled task</strong> active","<strong %s>%d scheduled tasks</strong> active",counts.cron),'class="modal-link" data-toggle="modal" data-remote="/modals/system-cron-modal.lp" data-id="system-cron-modal"',counts.cron)
html[#html+1] = '</span>'
html[#html+1] = '<span class="simple-desc">'
html[#html+1] = '<i class="icon-play-sign status-icon"></i>'
html[#html+1] = format(N("<strong %s>%d init script</strong> found","<strong %s>%d init scripts</strong> found",counts.init),'class="modal-link" data-toggle="modal" data-remote="/modals/system-init-modal.lp" data-id="system-init-modal"',counts.init)
html[#html+1] = '</span>'

local data = {
  html = html
}

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
