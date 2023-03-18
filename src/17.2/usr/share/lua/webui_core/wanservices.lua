local ipairs, string = ipairs, string
local find, match, gmatch = string.find, string.match, string.gmatch
local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local M = {}

-- Getting all available reserved ports and protocols
-- @param wanServices table of reserved services names, ports and protocols
-- @return reserved ports and protcols, ports as separate tables
local function reservedPortProtocols(wanServices)
  local wanServPortProto = {}
  for _, v in ipairs(wanServices) do
    if v.ports and v.proto then
      for resPort in v.ports:gmatch("%S+") do
        wanServPortProto[resPort] =  v.proto
      end
    end
  end
  return wanServPortProto
end

-- Parsing the ranged ports/port values and returning portStart, portEnd values
-- @param port either a port value like 5050 or port range value separated with colon like 5050:5055
-- @return portStart and portEndValues
local function parsePortRange(port)
  local portStart, portEnd = match(port, "^(%d+):(%d+)$")
  if not portStart then
    portStart, portEnd = port, port
  end
  return portStart, portEnd
end

-- Check whether the portmap protocol is equal to tcpudp and the reserved protcol contains either tcp or udp for comparison or
-- comparing the portmap protocol with reserved protocol
-- @param resProto the reserved services protocol
-- @param portMapValues table of values posted from Portmap table index which contains the protocol
-- @return true if found the portmap protcol in reserved protocol
local function protocolsMatch(resProto, portMapValues)
  return (portMapValues.protocol == "tcpudp" and (resProto == "tcp" or resProto == "udp")) or (resProto == portMapValues.protocol)
end

-- Check whether the input port and protocol are matching with the reserved ports and protocols
-- @param port WAN port value either a number or range with colon(:) separated
-- @param wanServ table of reserved services names, ports and protocols
-- @param portMapValues table of values posted from Portmap table index
-- @return nil+error if port/port range consists reserved port) and protocol matches reserved port and protocol else true
local function isReservedPort(port, portMapValues, wanServ)
  local portStart, portEnd = parsePortRange(port)
  for resPort, resProto in pairs(wanServ) do
    if (portStart <= resPort and resPort <= portEnd) then
      if protocolsMatch(resProto, portMapValues) then
        return nil, T"Port and Protocol are already Reserved"
      end
    end
  end
  return true
end

-- Checks whether the given port with protocol are reserved or not
-- @return nil+error or true with reserved ports table
function M.isPortAndProtoReserved()
  local wanServices = content_helper.convertResultToObject("uci.system.wan-service.", proxy.get("uci.system.wan-service."))
  local reserved = reservedPortProtocols(wanServices)
  local function isReserved(ports, portMapValues)
    return isReservedPort(ports, portMapValues, reserved)
  end
  return isReserved, reserved
end

return M
