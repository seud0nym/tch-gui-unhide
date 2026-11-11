local find,format,gmatch,match = string.find,string.format,string.gmatch,string.match
local concat,sort = table.concat,table.sort

local M = {}

local function words(s)
  local list = {}
  for w in gmatch(s or "","%S+") do
    list[#list+1] = w
  end
  return list
end

--- Works out the other changes needed when a Wi-Fi network (wifi-iface) moves to another interface.
-- netifd attaches an SSID to the interface named by its wifi-iface "network" option. The SSID must
-- also leave the ifname list of any other interface (the stock Guest interfaces list their SSID there),
-- and an interface that is not a bridge can only hold one device.
-- @param ssid the wifi-iface section name (e.g. "wl0_1")
-- @param new_net the interface the SSID moves to
-- @param intfs table: interface name -> { type = <uci type>, ifname = <uci ifname> }, as before the change
-- @param nets table: wifi-iface section name -> network, as before the change
-- @return list of { path, value } to set, sorted by path
function M.plan(ssid,new_net,intfs,nets)
  local sets = {}
  for name,intf in pairs(intfs) do
    if name ~= new_net then
      local kept,found = {},false
      for _,w in ipairs(words(intf.ifname)) do
        if w == ssid then
          found = true
        else
          kept[#kept+1] = w
        end
      end
      if found then
        sets[#sets+1] = { format("uci.network.interface.@%s.ifname",name), concat(kept," ") }
      end
    end
  end
  local target = intfs[new_net]
  if target and target.type ~= "bridge" then
    local members,count,listed = {},0,false
    local function add(dev)
      if not members[dev] then
        members[dev] = true
        count = count + 1
      end
    end
    for _,w in ipairs(words(target.ifname)) do
      if w == ssid then
        listed = true
      end
      add(w)
    end
    for other,net in pairs(nets) do
      if other ~= ssid and net == new_net then
        add(other)
      end
    end
    add(ssid)
    if count > 1 then
      sets[#sets+1] = { format("uci.network.interface.@%s.type",new_net), "bridge" }
    elseif not listed then
      -- The SSID is the interface's only device: list it, as the stock Guest interfaces do
      sets[#sets+1] = { format("uci.network.interface.@%s.ifname",new_net), ssid }
    end
  end
  sort(sets,function(a,b) return a[1] < b[1] end)
  return sets
end

--- Counts the Wi-Fi networks (wifi-iface sections) attached to an interface by their "network" option.
-- @param nets table: wifi-iface section name -> network
-- @param intf the interface name
function M.count_ssids(nets,intf)
  local count = 0
  for _,net in pairs(nets) do
    if net == intf then
      count = count + 1
    end
  end
  return count
end

--- Whether netifd attaches SSIDs to interfaces through the wifi-iface "network" option on this firmware.
-- Only firmware with a netifd wireless handler does (17.2 to 20.3.c); 20.4 and 21.4 have none.
function M.supported()
  local lfs = require("lfs")
  return lfs.attributes("/lib/netifd/wireless/broadcom.sh","mode") == "file"
end

--- Sorts the access-point SSIDs into those the Wi-Fi Networks tab can move and those it shows read-only.
-- Not listed at all: station and backhaul interfaces, hotspot SSIDs (hotspotd and gre_hotspotd) and EasyMesh
-- backhaul ("BH-") SSIDs. Read-only: SSIDs the EasyMesh agent (multiap) manages, SSIDs whose network option is a
-- list, and SSIDs on an existing interface the tab does not offer. An SSID whose network is empty or names no
-- interface is movable, so that it can be repaired.
-- @param proxy the datamodel
-- @param intfs table: interface name -> { type, ifname }, the interfaces the tab offers
-- @return table: wifi-iface section name -> "movable" or "readonly"; and the wifi-iface objects by name
function M.classify(proxy,intfs)
  local content_helper = require("web.content_helper")
  local untaint = string.untaint
  local hotspot = {}
  for _,v in ipairs(content_helper.convertResultToObject("uci.hotspotd.wifi-iface.@.",proxy.get("uci.hotspotd.wifi-iface."))) do
    hotspot[untaint(v.iface)] = true
  end
  for _,v in ipairs(proxy.get("uci.gre_hotspotd.hotspot.") or {}) do
    if find(v.path,"%.wifi_iface%.") and v.param == "value" then
      hotspot[untaint(v.value)] = true
    end
  end
  local managed = {}
  local agent = proxy.get("uci.multiap.agent.enabled","uci.multiap.agent.bss_list")
  if agent and agent[1].value == "1" then
    for iface in gmatch(untaint(agent[2].value),"[^,%s]+") do
      managed[iface] = true
    end
  end
  local exists = {}
  for _,v in ipairs(proxy.getPN("uci.network.interface.",true) or {}) do
    local name = match(v.path,"@([^%.]+)%.$")
    if name then
      exists[untaint(name)] = true
    end
  end
  local ifaces,class = {},{}
  for _,v in ipairs(content_helper.convertResultToObject("uci.wireless.wifi-iface.@.",proxy.get("uci.wireless.wifi-iface."))) do
    local iface,net = untaint(v.paramindex),untaint(v.network)
    ifaces[iface] = v
    if v.mode == "ap" and v.backhaul ~= "1" and not hotspot[iface] and untaint(v.ssid):sub(1,3) ~= "BH-" then
      if managed[iface] or find(net,"%s") then
        class[iface] = "readonly"
      elseif intfs[net] or net == "" or not exists[net] then
        class[iface] = "movable"
      else
        class[iface] = "readonly"
      end
    end
  end
  return class,ifaces
end

return M
