local proxy = require ("datamodel")
local ui_helper = require ("web.ui_helper")
local content_helper = require ("web.content_helper")
local find,format,match = string.find,string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local sipprofile_uci_path = "uci.mmpbxrvsipnet.profile."
local sipprofile_rpc_path = "rpc.mmpbx.sip_profile.@"
local profile_rpc_path = "rpc.mmpbx.profile.@"
local profilestats_rpc_path = "rpc.mmpbx.profilestatistics.@"

local modal_link = 'class="modal-link" data-toggle="modal" data-remote="modals/mmpbx-profile-modal.lp" data-id="mmpbx-profile-modal"'

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

local function toNumberOrZero(data)
  local value
  if data and data[1] then
    value = tonumber(data[1].value)
  end
  if not value then
    value = 0
  end
  return value
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
  local html = {}

  html[#html+1] = '<span class="modal-link" data-toggle="modal" data-remote="modals/mmpbx-global-modal.lp" data-id="mmpbx-profile-modal">'
  html[#html+1] = ui_helper.createSimpleLight(mmpbx_state,status[untaint(mmpbx_state)])
  html[#html+1] = '</span>'

  local todvoicednd_content = proxy.get("uci.tod.todvoicednd.enabled","uci.tod.todvoicednd.ringing")
  local todvoicednd_info = {}
  html[#html+1] = '<span class="modal-link" data-toggle="modal" data-remote="modals/tod_dnd-modal.lp" data-id="tod_dnd-modal">'
  if todvoicednd_content and todvoicednd_content[1].value == "1" then
    local state = "enabled"
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
              state = "active"
            elseif state == "enabled" then
              state = "inactive"
            end
          end
        end
      end
    end
    html[#html+1] = ui_helper.createSimpleLight("1",T("Do Not Disturb "..state))
  else
    html[#html+1] = ui_helper.createSimpleLight("0",T"Do Not Disturb disabled")
  end
  html[#html+1] = '</span>'

  local sipprofile_content = content_helper.getMatchedContent(sipprofile_uci_path)
  local sipprofile_info = {}
  for _,v in pairs(sipprofile_content) do
    local name = match(v.path,"@([^%.]+)")
    if v.display_name ~= nil and v.display_name ~= "" then
      sipprofile_info[name] = v.display_name
    else
      sipprofile_info[name] = v.uri
    end
  end

  local disabled = 0
  for name,value in spairs(sipprofile_info) do
    local enabled = proxy.get(sipprofile_rpc_path..name..".enabled")
    if enabled and enabled[1].value == "1" then
      local state = proxy.get(profile_rpc_path..name..".sipRegisterState")
      if state then
        if state[1].value == "Registered" then
          local incoming = proxy.get(profilestats_rpc_path..name..".incomingCalls")
          local incomingConnected = proxy.get(profilestats_rpc_path..name..".incomingCallsConnected")
          local outgoing = proxy.get(profilestats_rpc_path..name..".outgoingCalls")
          local callsIn = 0
          local callsMissed = 0
          local callsOut = 0
          if not incoming then
            incoming = proxy.get(profilestats_rpc_path..name..".IncomingCallsReceived") -- 20.3.c
          end
          if not incomingConnected then
            incomingConnected = proxy.get(profilestats_rpc_path..name..".IncomingCallsConnected") -- 20.3.c
          end
          if incoming and incomingConnected then
            callsIn = toNumberOrZero(incomingConnected)
            callsMissed = toNumberOrZero(incoming) - callsIn
          end
          if outgoing then
            callsOut = toNumberOrZero(outgoing)
          else
            outgoing = proxy.get(profilestats_rpc_path..name..".OutgoingCallsAttempted") -- 20.3.c
            if outgoing then
              callsOut = toNumberOrZero(outgoing)
            end
          end
          html[#html+1] = format('<span %s>',modal_link)
          if todvoicednd_info[name] or todvoicednd_info["All"] then
            html[#html+1] = ui_helper.createSimpleLight("3",format("%s registered <sup style='font-size:xx-small;'>(Do Not Disturb)</sup>",value))
          else
            html[#html+1] = ui_helper.createSimpleLight("1",T(value.." registered"))
          end
          html[#html+1] = '</span>'
          html[#html+1] = format('<p class="subinfos modal-link" data-toggle="modal" data-remote="/modals/mmpbx-log-modal.lp" data-id="mmpbx-log-modal">Calls: %s <small>IN</small> %s <small>MISSED</small> %s <small>OUT</small></p>',callsIn,callsMissed,callsOut)
        elseif state[1].value == "Registering" then
          html[#html+1] = format('<span %s>',modal_link)
          html[#html+1] = ui_helper.createSimpleLight("2",value..T" registering")
          html[#html+1] = '</span>'
        else
          html[#html+1] = format('<span %s>',modal_link)
          if mmpbx_state == "1" then
            html[#html+1] = ui_helper.createSimpleLight("4",value..T" unregistered")
          else
            html[#html+1] = ui_helper.createSimpleLight("0",value..T" unregistered")
          end
          html[#html+1] = '</span>'
        end
      else
        html[#html+1] = format('<span %s>',modal_link)
        html[#html+1] = ui_helper.createSimpleLight("0",value..T" unregistered")
        html[#html+1] = '</span>'
      end
    else
      disabled = disabled + 1
    end
  end

  return html,disabled
end

return M
