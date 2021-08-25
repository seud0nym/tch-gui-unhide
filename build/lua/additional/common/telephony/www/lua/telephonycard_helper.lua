local proxy = require ("datamodel")
local ui_helper = require ("web.ui_helper")
local content_helper = require ("web.content_helper")
local format, lower = string.format, string.lower

local sipprofile_uci_path = "uci.mmpbxrvsipnet.profile."
local sipprofile_rpc_path = "rpc.mmpbx.sip_profile.@"
local profile_rpc_path = "rpc.mmpbx.profile.@"
local profilestats_rpc_path = "rpc.mmpbx.profilestatistics.@"

local modal_link='class="modal-link" data-toggle="modal" data-remote="modals/mmpbx-profile-modal.lp" data-id="mmpbx-profile-modal"'

local function spairs(t, order)
  -- collect the keys
  local keys = {}
  for k in pairs(t) do keys[#keys+1] = k end

  -- if order function given, sort by it by passing the table and keys a, b,
  -- otherwise just sort the keys 
  if order then
      table.sort(keys, function(a,b) return order(t, a, b) end)
  else
      table.sort(keys)
  end

  -- return the iterator function
  local i = 0
  return function()
      i = i + 1
      if keys[i] then
          return keys[i], t[keys[i]]
      end
  end
end

local M = {}

function M.getTelephonyCardHTML(mmpbx_state) 
  local sipprofile_content = content_helper.getMatchedContent(sipprofile_uci_path)
  local sipprofile_info = {}
  local v = {}
  for _, v in pairs(sipprofile_content) do
    local name = string.match (v.path, "@([^%.]+)")
    if v.display_name ~= nil and v.display_name ~= "" then
      sipprofile_info[name] = v.display_name
    else
      sipprofile_info[name] = v.uri
    end
  end

  local html = {}

  local name, value = "", ""
  local disabled = 0
  for name, value in spairs(sipprofile_info) do
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
            callsIn = incomingConnected[1].value
            callsMissed = incoming[1].value - callsIn
          end
          if outgoing then
            callsOut = outgoing[1].value
          else
            outgoing = proxy.get(profilestats_rpc_path..name..".OutgoingCallsAttempted") -- 20.3.c
            if outgoing then
              callsOut = outgoing[1].value
            end
          end
          html[#html+1] = format('<span %s>', modal_link)
          html[#html+1] = ui_helper.createSimpleLight("1", value .. T" registered")
          html[#html+1] = '</span>'
          html[#html+1] = format('<p class="subinfos modal-link" data-toggle="modal" data-remote="/modals/mmpbx-log-modal.lp" data-id="mmpbx-log-modal">Calls: %s <small>IN</small> %s <small>MISSED</small> %s <small>OUT</small></p>', callsIn, callsMissed, callsOut)
        elseif state[1].value == "Registering" then
          html[#html+1] = format('<span %s>', modal_link)
          html[#html+1] = ui_helper.createSimpleLight("2", value .. T" registering")
          html[#html+1] = '</span>'
        else
          html[#html+1] = format('<span %s>', modal_link)
          if mmpbx_state == "1" then
            html[#html+1] = ui_helper.createSimpleLight("4", value .. T" unregistered")
          else
            html[#html+1] = ui_helper.createSimpleLight("0", value .. T" unregistered")
          end
          html[#html+1] = '</span>'
        end
      else
        html[#html+1] = format('<span %s>', modal_link)
        html[#html+1] = ui_helper.createSimpleLight("0", value .. T" unregistered")
        html[#html+1] = '</span>'
      end
    else
      disabled = disabled + 1
    end
  end

  return html, disabled
end

return M
