#! /usr/bin/lua
local lfs = require("lfs")
local proxy = require("datamodel")

local datadir = "/root/trafficmon/"
local histdir = datadir.."history/"

local debug = false

if arg then
  local a
  for _,a in ipairs(arg) do
    if a == "-d" then
      debug = true
    end
  end
end

local function DataAggregator(datadir, histdir)
  local format, match, sub = string.format, string.match, string.sub

  if not lfs.attributes(histdir, "mode") then
    if debug then 
      print("Creating directory "..histdir) 
    end
    lfs.mkdir(histdir)
  elseif debug then 
    print("Directory "..histdir.." exists")
  end

  local oldest = os.date("%F", os.time()-8640000) -- 100 days
  if debug then 
    print("Oldest date to retain is "..oldest) 
  end
  local summaryfilename
  for summaryfilename in lfs.dir(histdir) do
    if sub(summaryfilename,1,1) ~= "." then
      if summaryfilename < oldest then
        if debug then 
          print(" - Removing "..histdir..summaryfilename) 
        end
        os.execute("logger -t traffichistory -p 135 Removing "..histdir..summaryfilename)
        os.remove(histdir .. summaryfilename)
        proxy.set("rpc.gui.traffichistory.remove", summaryfilename)
      end
    elseif debug then 
      print(" - Ignoring "..summaryfilename)
    end
  end

  local checkdataname = datadir .. "check_data"
  local checkdata = {}

  local idx = 0
  if debug then 
    print("Reading "..checkdataname) 
  end
  local file = io.open(checkdataname, "r")
  if file then
    local pattern = "([^|]*)|([^|]*)|(%d+)|(%d+)"
    local i = 0
    local line
    for line in file:lines() do
      i = i + 1
      if i > 1 then
        local name, dtype, carry, number = match(line, pattern)
        if name then
          if debug then 
            print(" "..i.." Found "..name.." "..dtype) 
          end
          checkdata[name] = checkdata[name] or {}
          checkdata[name][dtype] = number
        elseif debug then 
          print(" "..i.." Ignored - name is nil?")
        end
      else
        idx = tonumber(line)
        if debug then 
          print("Current index is "..idx) 
        end
      end
    end
    if debug then 
      print("Closing "..checkdataname) 
    end
    file:close()
  else
    if debug then 
      print("ERROR: Failed to open "..checkdataname) 
    end
    os.execute("logger -t traffichistory -p 131 Failed to open "..checkdataname)
  end

  local lastupdate = lfs.attributes(checkdataname, "modification") - (idx * 120)
  local datetable = os.date("*t", lastupdate)
  local sincemidnight
  local filename
  if datetable.min == 0 and datetable.hour == 0 then
    sincemidnight = 86400
    filename = os.date("%F", lastupdate - 60)
  else
    sincemidnight = (datetable.min * 60) + (datetable.hour * 3600)
    filename = os.date("%F", lastupdate)
  end
  local todaylines = math.floor((sincemidnight / 600) + 0.05)
  if debug then 
    print("Target filename is "..filename) 
  end

  local interface, data, dtype
  local summary = {}
  for interface, data in pairs(checkdata) do
    summary[interface] = summary[interface] or {}
    for dtype, _ in pairs(data) do
      local source = datadir .. interface .. "_" .. dtype
      if lfs.attributes(source, "modification") >= lastupdate then
        if debug then 
          print("Reading "..source) 
        end
        local file = io.open(source, "r")
        if file then
          local bytes = {}
          local line
          for line in file:lines() do
            bytes[#bytes+1] = line
          end
          if debug then 
            print(" - Closing "..source.." after reading "..#bytes.." lines") 
          end
          file:close()
          local firstline = (#bytes - todaylines) + 1
          if firstline < 2 then
            firstline = 2
          end
          if debug then 
            print(" - About to process lines "..firstline.." to "..#bytes) 
          end
          local total = 0
          local i
          for i = firstline,#bytes,1  do
            if debug then 
              print(" - Accumulating record #"..i) 
            end
            total = total + tonumber(bytes[i])
          end
          if debug then 
            print("Setting "..interface.." "..dtype.." to "..total) 
          end
          summary[interface][dtype] = total
        else
          if debug then 
            print("ERROR: Failed to open "..source) 
          end
          os.execute("logger -t traffichistory -p 131 Failed to open "..source)
        end
      else
        if debug then 
          print("Ignored "..source.." - file is older than lastupdate: "..os.date("%c", lfs.attributes(source, "modification")).." < "..os.date("%c",lastupdate))
          print("Setting "..interface.." "..dtype.." to 0") 
        end
        summary[interface][dtype] = 0
    end
    end
  end

  if debug then 
    print("Writing "..histdir..filename) 
  end
  local target = io.open(histdir .. filename, "w")
  if target then
    local pattern = "%s|%.0f|%.0f\n"
    for interface, data in pairs(summary) do
      if data and data.rx_bytes and data.tx_bytes then
        if debug then 
          print(" -> "..interface.." rx: "..data.rx_bytes.." tx: "..data.tx_bytes) 
        end
        target:write(format(pattern, interface, data.rx_bytes, data.tx_bytes))
      elseif debug then 
        print(" -> "..interface.." ignored??? - rx_bytes or tx_bytes not found?")
      end
    end
    if debug then 
      print("Closing "..histdir..filename) 
    end
    target:close()
    os.execute("logger -t traffichistory -p 134 Refreshed "..histdir..filename)
    proxy.set("rpc.gui.traffichistory.refresh", filename)
  else
    if debug then 
      print("ERROR: Failed to write "..histdir..filename) 
    end
    os.execute("logger -t traffichistory -p 131 Failed to open "..histdir..filename)
  end
end

-- lock file directory
local lock = lfs.lock_dir(datadir)
if lock then
  pcall(DataAggregator, datadir, histdir)
  -- unlock file directory
  lock:free()
else
  if debug then 
    print("Failed to acquire local on "..datadir) 
  end
  os.execute("logger -t traffichistory -p 132 Failed to acquire lock on "..datadir)
end
