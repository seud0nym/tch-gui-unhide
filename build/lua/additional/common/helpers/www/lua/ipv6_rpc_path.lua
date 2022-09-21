local proxy = require("datamodel")
local paths = {
  "rpc.network.interface.@wan6.",
  "rpc.network.interface.@wan.",
  "rpc.network.interface.@wan_6.",
  "rpc.network.interface.@6rd.",
}

local M = {}

function M.getPath(suffix)
  for _,path in ipairs(paths) do
    local ip6addr = proxy.get(path.."ip6addr")
    if ip6addr and ip6addr[1].value ~= "" then
      return path..(suffix or ""), ip6addr
    end
  end
  return paths[1]..(suffix or "")
end

return M
