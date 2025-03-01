-- Localization
gettext.textdomain('webui-core')

local log = require("tch.logger").new("proxy_helper",7)
local message_helper = require("web.uimessage_helper")
local proxy = require("datamodel")

local ipairs,string = ipairs,string
local format,gsub,sub = string.format,string.gsub,string.sub

local M = {}

function M.add(p,v,quiet)
  log:notice("Adding '%s' to %s",v or "new section",p)
  local key,error = proxy.add(p,v)
  if not key then
    log:error("Failed to add '%s' to %s: %s",v or "new section",p,error)
    if not quiet then
      message_helper.pushMessage(T(format("Failed to add %s to '%s': %s",v or "new section",p,error)),"error")
    end
    return key,error
  else
    if proxy.getPN(p..key..".",true) then -- key is param
      key = key
    elseif proxy.getPN(p.."@"..key..".",true) then -- key is param without leading @
      key = key
    else -- probably _key
      local sections = proxy.getPN(p,true)
      local section = proxy.get(sections[#sections].path.."_key")
      if section then -- mapping contains _key
        if section[1].value == key then -- last record matches _key
          proxy.set(section[1].path..section[1].param,"")
          key = gsub(gsub(section[1].path,p,""),"%.","")
        else -- last record does not match _key, so search (backwards) for it
          error = "Unknown error - could not locate added key "..key
          for i=#sections,1,-1 do
            section = proxy.get(sections[i].path.."_key")
            if section[1].value == key then -- found matching key
              key = gsub(gsub(section[1].path,p,""),"%.","")
              error = nil
              break
            end
          end
        end
      else -- mapping does not contain _key, so just assume it is the last one
        local empty = true
        for _,value in ipairs(proxy.get(sections[#sections].path)) do
          empty = empty and value == ""
          if not empty then
            break
          end
        end
        if empty then
          key = gsub(gsub(sections[#sections].path,p,""),"%.","")
        else
          key,error = nil,"Add successful but failed to determine key!"
        end
      end
    end
  end
  if key and sub(key,1,1) == "@" then
    key = sub(key,2)
  end
  if error then
    log:error("Failed to add '%s' to %s : %s",v or "new section",p,error)
  else
    log:notice("Added key '%s' to %s",key,p)
  end
  return key,error
end

function M.set(p,v)
  log:notice("Setting %s to '%s'",p,v or "nil")
  local success,errors = proxy.set(p,v)
  if not success then
    for _,err in ipairs(errors) do
      log:error("Failed to set %s to '%s': %s (%s)",err.path,v,err.errmsg,err.errcode)
      message_helper.pushMessage(T(format("Failed to set %s to '%s': %s (%s)",err.path,v,err.errmsg,err.errcode)),"error")
    end
  end
  return success,errors
end

return M
