local proxy = require ("datamodel")
local ui_helper = require ("web.ui_helper")
local content_helper = require ("web.content_helper")
local find,format,match = string.find,string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local modal_link = 'class="modal-link" data-toggle="modal" data-remote="modals/mmpbx-profile-modal.lp" data-id="mmpbx-profile-modal"'
local stats_fmt = '<p class="subinfos modal-link" style="font-size:11px;line-height:9px;" data-toggle="modal" data-remote="/modals/mmpbx-log-modal.lp" data-id="mmpbx-log-modal">Calls: %s <small>IN</small> %s <small>MISSED</small> %s <small>OUT</small></p>'

local function spairs(t,order)
  -- collect the keys
  local keys = {}
  for k in pairs(t) do keys[#keys+1] = k end

  -- if order function given,sort by it by passing the table and keys a,b,
  -- otherwise just sort the keys
  if order then
      table.sort(keys,function(a,b) return order(t,a,b) end)
  else
      table.sort(keys)
  end

  -- return the iterator function
  local i = 0
  return function()
      i = i + 1
      if keys[i] then
          return keys[i],t[keys[i]]
      end
  end
end

local function toMinutes(time)
  local h,m = match(untaint(time),"(%d+):(%d+)")
  return ((tonumber(h) or 0) * 60) + (tonumber(m) or 0)
end

local status = {
  ["0"] = T"Telephony disabled",
  ["1"] = T"Telephony enabled",
}

local M = {}

function M.getTelephonyCardHTML(mmpbx_state)
  local temp = {}

  local sipprofile_content = content_helper.getMatchedContent("uci.mmpbxrvsipnet.profile.")
  local sipprofile_info = {}
  for _,v in pairs(sipprofile_content) do
    local name = match(v.path,"@([^%.]+)")
    sipprofile_info[name] = { uri = untaint(v.uri) }
    if v.display_name ~= nil and v.display_name ~= "" then
      sipprofile_info[name].display_name = v.display_name
    else
      sipprofile_info[name].display_name = v.uri
    end
  end

  local todvoicednd_content = proxy.get("uci.tod.todvoicednd.enabled","uci.tod.todvoicednd.ringing")
  local todvoicednd_info = {}
  local todvoicednd_state = "0"
  local todvoicednd_text = "disabled"
  if todvoicednd_content and todvoicednd_content[1].value == "1" then
    todvoicednd_state = "1"
    todvoicednd_text = "enabled"
    local ringing = untaint(todvoicednd_content[2].value)
    local today = os.date("%a")
    local now = toMinutes(os.date("%H:%M"))
    for _,p in ipairs(proxy.get("uci.tod.voicednd.")) do 
      local key = match(untaint(p.path),"^uci%.tod%.voicednd%.@voicednd([^%.]+)%.profile")
      if key then
        local profile = untaint(p.value)
        local timers = proxy.get("uci.tod.timer.@timer"..key..".")
        if timers then
          local v = {}
          for _,t in ipairs(timers) do
            v[untaint(t.param)] = t.value
          end
          if v.enabled ~= "0" then
            local start_days,start_time = match(untaint(v.start_time),"([^:]*):([%d:]*)")
            local stop_days,stop_time = match(untaint(v.stop_time),"([^:]*):([%d:]*)")
            local start = toMinutes(start_time)
            local stop = toMinutes(stop_time)
            local active = find(start_days,today,0,true) and find(stop_days,today,0,true) and now >= start and now <= stop
            local dnd = (ringing == "off" and active) or (ringing == "on" and not active)
            if not todvoicednd_info[profile] then
              todvoicednd_info[profile] = dnd
            else
              todvoicednd_info[profile] = todvoicednd_info[profile] or dnd
            end
            if dnd then
              todvoicednd_text = "active"
            elseif todvoicednd_text == "enabled" then
              todvoicednd_state = "0"
              todvoicednd_text = "enabled (inactive)"
            end
          end
        end
      end
    end
  end

  local stats = {}
  local calllog = proxy.getPN("rpc.mmpbx.calllog.info.",true)
  for _,p in ipairs(calllog) do
    local v = proxy.get(p.path.."Localparty",p.path.."Direction",p.path.."connectedTime")
    if v then
      local uri = untaint(v[1].value)
      local direction = untaint(v[2].value)
      local duration = untaint(v[3].value)
      if uri == "" then
        uri = "VoLTE"
      end
      if not stats[uri] then
        stats[uri] = { i = 0, m = 0, o = 0 }
      end
      if direction == "2" then
        stats[uri].o = stats[uri].o + 1
      else
        stats[uri].i = stats[uri].i + 1
        if duration == "0" then
          stats[uri].m = stats[uri].m + 1
        end
      end
    end
  end

  local profiles = 0
  for name,p in spairs(sipprofile_info) do
    local details = proxy.get("rpc.mmpbx.sip_profile.@"..name..".enabled")
    if details and details[1].value == "1" then
      profiles = profiles + 1
      local state = proxy.get("rpc.mmpbx.profile.@"..name..".sipRegisterState")
      if state then
        if state[1].value == "Registered" then
          local uri_stats = stats[p.uri] or { i = 0, m = 0, o = 0 }
          local callsMissed = uri_stats.m
          local callsIn = uri_stats.i - callsMissed
          local callsOut = uri_stats.o
          temp[#temp+1] = format('<span %s>',modal_link)
          if todvoicednd_info[name] or todvoicednd_info["All"] then
            temp[#temp+1] = ui_helper.createSimpleLight("3",format("%s registered <sup style='font-size:xx-small;'>(Do Not Disturb)</sup>",p.display_name))
          else
            temp[#temp+1] = ui_helper.createSimpleLight("1",T(p.display_name.." registered"))
          end
          temp[#temp+1] = '</span>'
          temp[#temp+1] = format(stats_fmt,callsIn,callsMissed,callsOut)
        elseif state[1].value == "Registering" then
          temp[#temp+1] = format('<span %s>',modal_link)
          temp[#temp+1] = ui_helper.createSimpleLight("2",p.display_name..T" registering")
          temp[#temp+1] = '</span>'
        else
          temp[#temp+1] = format('<span %s>',modal_link)
          if mmpbx_state == "1" then
            temp[#temp+1] = ui_helper.createSimpleLight("4",p.display_name..T" unregistered")
          else
            temp[#temp+1] = ui_helper.createSimpleLight("0",p.display_name..T" unregistered")
          end
          temp[#temp+1] = '</span>'
        end
      else
        temp[#temp+1] = format('<span %s>',modal_link)
        temp[#temp+1] = ui_helper.createSimpleLight("0",p.display_name..T" unregistered")
        temp[#temp+1] = '</span>'
      end
    end
  end

  local volte = {
    state = "Device.Services.X_TELSTRA_VOLTE.Enable",
    registration_status = "rpc.mobiled.device.@1.voice.info.volte.registration_status",
    cs_emergency = "rpc.mobiled.device.@1.voice.network_capabilities.cs.emergency",
    volte_emergency = "rpc.mobiled.device.@1.voice.network_capabilities.volte.emergency",
  }
  content_helper.getExactContent(volte)
  if volte and volte.state == "1" then
    local uri_stats = stats["VoLTE"] or { i = 0, m = 0, o = 0 }
    local callsMissed = uri_stats.m
    local callsIn = uri_stats.i - callsMissed
    local callsOut = uri_stats.o
    local showStats = true
    profiles = profiles + 1
    temp[#temp+1] = '<span class="modal-link" data-toggle="modal" data-remote="/modals/mmpbx-volte-modal.lp" data-id="mmpbx-volte-modal">'
    if mmpbx_state == "1" then
      if volte.registration_status == "registered" then
        temp[#temp+1] = ui_helper.createSimpleLight("1",T"VoLTE working normally")
      elseif volte.cs_emergency == "true" or volte.volte_emergency == "true" then
        temp[#temp+1] = ui_helper.createSimpleLight("2",T"VoLTE emergency calls only")
      else
        temp[#temp+1] = ui_helper.createSimpleLight("4",T"VoLTE not connected")
        showStats = false
      end
    else
      temp[#temp+1] = ui_helper.createSimpleLight("0",T"VoLTE unavailable")
      showStats = false
    end
    temp[#temp+1] = '</span>'
    if showStats then
      temp[#temp+1] = format(stats_fmt,callsIn,callsMissed,callsOut)
    end
  end

  local html = {}

  if profiles < 3 then
    html[#html+1] = '<span class="modal-link" data-toggle="modal" data-remote="modals/mmpbx-global-modal.lp" data-id="mmpbx-profile-modal">'
    html[#html+1] = ui_helper.createSimpleLight(mmpbx_state,status[untaint(mmpbx_state)])
    html[#html+1] = '</span>'
  end

  if profiles > 0 then
    if #html == 0 then
      html = temp
    else
      for i = 1,#temp do
        html[#html+1] = temp[i]
      end
    end
  elseif mmpbx_state == "1" then
    html[#html+1] = format('<span %s>',modal_link)
    html[#html+1] = ui_helper.createSimpleLight("2",T"No SIP profiles or VoLTE enabled")
    html[#html+1] = '</span>'
  end

  if profiles < 4 then
    html[#html+1] = '<span class="modal-link" data-toggle="modal" data-remote="modals/tod_dnd-modal.lp" data-id="tod_dnd-modal">'
    html[#html+1] = ui_helper.createSimpleLight(todvoicednd_state,T("Do Not Disturb "..todvoicednd_text))
    html[#html+1] = '</span>'
  end

  --region DECT (Do not remove this comment - used by 120-Telephony)
  if profiles < 5 then
    local dect = {
      state = "rpc.mmpbx.dectemission.state",
    }
    content_helper.getExactContent(dect)
    if dect and (dect.state == "1" or dect.state == "0")  then
      html[#html+1] = '<span class="modal-link" data-toggle="modal" data-remote="/modals/mmpbx-dect-modal.lp" data-id="mmpbx-dect-modal">'
      if dect.state == "1" then
        html[#html+1] = ui_helper.createSimpleLight(dect.state,T"DECT Emission Mode enabled")
      else
        html[#html+1] = ui_helper.createSimpleLight(dect.state,T"DECT Emission Mode disabled")
      end
      html[#html+1] = '</span>'
    end
  end
  --endregion (Do not remove this comment - used by 120-Telephony)

  return html
end

return M
