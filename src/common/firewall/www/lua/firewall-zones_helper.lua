local proxy = require("datamodel")
local message_helper = require("web.uimessage_helper")
local format,gsub,match = string.format,string.gsub,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local vNES = require("web.post_helper").validateNonEmptyString
local uci_path = "uci.firewall.zone."

local M = {}

function M.onZoneAdded(index,data)
  local key

  if proxy.getPN(uci_path..index..".",true) then -- key is param
    return
  elseif proxy.getPN(uci_path.."@"..index..".",true) then -- key is param without leading @
    key = "@"..index
  else -- probably _key
    local sections = proxy.getPN(uci_path,true)
    local section = proxy.get(sections[#sections].path.."_key")
    if section then -- mapping contains _key
      if section[1].value == index then -- last record matches _key
        proxy.set(section[1].path..section[1].param,"")
        key = gsub(gsub(section[1].path,uci_path,""),"%.","")
      else -- last record does not match _key, so search (backwards) for it
        for i=#sections,1,-1 do
          section = proxy.get(sections[i].path.."_key")
          if section[1].value == index then -- found matching key
            key = gsub(gsub(section[1].path,uci_path,""),"%.","")
            break
          end
        end
      end
    else -- mapping does not contain _key, so just assume it is the last one
      key = gsub(gsub(sections[#sections].path,uci_path,""),"%.","")
    end
  end

  if key then
    for k,v in pairs(data) do
      if k == "network" and type(v) ~= "table" then
        v = { v }
      end

      if type(v) == "table" then
        for _,value in ipairs(v) do
          local subkey,error = proxy.add(uci_path..key.."."..k..".")
          if not subkey then
            message_helper.pushMessage(T(string.format("Error adding %s to s list: %s",value,k,error)),"error")
          else
            proxy.set(uci_path..key.."."..k..".@"..subkey..".value",value)
          end
        end
      else
        proxy.set(uci_path..key.."."..k,v)
      end
    end

    proxy.apply()
  end
end

function M.onZoneDeleted(index)
  local paths = {}

  local name = proxy.get(uci_path.."@"..index..".name")
  if name then
    local zone = untaint(name[1].value)

    for _,v in ipairs(proxy.getPN("uci.firewall.rule.",true)) do
      local fw_rule = proxy.get(v.path.."src",v.path.."dest")
      if fw_rule[1].value == zone or fw_rule[2].value == zone then
        paths[#paths+1] = v.path
      end
    end

    for _,v in ipairs(proxy.getPN("uci.firewall.forwarding.",true)) do
      local fw_fwd = proxy.get(v.path.."src")
      if fw_fwd[1].value == zone then
        paths[#paths+1] = v.path
      end
    end
  end

  for i=#paths,1,-1 do
    local path = paths[i]
    local result,error = proxy.del(path)
    if not result then
      ngx.log(ngx.ERR,format("Failed to delete %s: %s",path,error))
      message_helper.pushMessage(format("Failed to remove %s: %s",path,error),"error")
    end
  end

  proxy.apply()
end

function M.validateName(value)
  local valid,errmsg = vNES(value)
  if not valid then
    return valid,errmsg
  elseif not match(untaint(value),"[_%w]+") then
    return nil,"Can only contain alphanumerics and underscores"
  end
  return true
end

return M