---@diagnostic disable: lowercase-global
json=require('dkjson')

if (#arg % 2 ~= 0) then
  print("Required arguments: hostname:port username:password")
  print("                    Repeat hostname:port username:password for multiple servers")
  os.exit()
end

targets = {}
for i = 1,#arg,2 do
  targets[#targets+1] = {
    address=arg[i],
    credentials=arg[i+1]
  }
end

function execute(cmd)
  local popen = io.popen(cmd)
  if popen then
    local response = popen:read('*a')
    popen:close()
    return response
  end
  return nil
end

function getKnownDevices()
  local devIPs = {}
  local allIPs = {}

  local devices = json.decode(execute('ubus call hostmanager.device get'))
  table.sort(devices,function(a,b)
    return string.lower(a.hostname) < string.lower(b.hostname)
  end)

  for _,host in pairs(devices) do
    if host.interface == "lan" then
      local hostname = host.hostname
      if not hostname or hostname == "" then
        hostname = host['user-friendly-name']
        if not hostname or hostname == "" then
          hostname = string.format("Unknown-%s",host['mac-address'])
        end
      end
      local last2char = string.sub(hostname,-2)
      if last2char == '-2' or last2char == '-3' or last2char == '-4' or last2char == '-5' then
        hostname = string.sub(hostname,1,-3)
      end
      devIPs[hostname] = {}
      for _,v4 in pairs(host.ipv4) do
        table.insert(devIPs[hostname],v4.address)
        allIPs[v4.address] = hostname
      end
      for _,v6 in pairs(host.ipv6) do
        table.insert(devIPs[hostname],v6.address)
        allIPs[v6.address] = hostname
      end
    end
  end

  local localhost = execute('echo -n $HOSTNAME') or "mymodem"
  devIPs[localhost] = {}
  for cidr in string.gmatch(execute('ip -o -br address show br-lan | tr -s " " | cut -d" " -f3-'),"([^%s]+)") do
    local address = string.match(cidr,"([^/]+)/%d+")
    table.insert(devIPs[localhost],address)
    allIPs[address] = localhost
  end

  return devIPs,allIPs
end

devIPs,allIPs = getKnownDevices()

function deduplicate(t)
  local duplicate = {}
  local result = {}

  for _,v in pairs(t) do
    if not duplicate[v] then
      result[#result+1] = v
      duplicate[v] = true
    end
  end

  table.sort(result, function (a,b)
    return string.lower(a) < string.lower(b)
  end)

  return result
end

function post(address,credentials,action,data)
  local json = json.encode(data)
  local result = execute(string.format('curl -su "%s" -X POST -H "Content-Type: application/json" --data \'%s\' http://%s/control/clients/%s',credentials,json,address,action))
  if result and #result > 0 then
    print("ERROR:",result)
  end
end

function the_same(original,updated)
  if original.count ~= #updated then
    return false
  end
  for _,v in pairs(updated) do
    if not original.hash[v] then
      return false
    end
  end
  return true
end

function process(target)
  local address = target.address
  local credentials = target.credentials

  local append = {}
  local delete = {}
  local update = {}

  local clients = {}
  local ids = {}
  local names = {}

  local clientids = {}

  local current = json.decode(execute(string.format('curl -su "%s" http://%s/control/clients',credentials,address)) or {})

  if current.clients then
    table.sort(current.clients,function(a,b)
      return string.lower(a.name) < string.lower(b.name)
    end)
    for _,client in pairs(current.clients) do
      if client.ids then
        local hostname = client.name
        clientids[hostname] = {
          count = #client.ids,
          hash = {},
        }
        for _,id in ipairs(client.ids)  do
          clientids[hostname]["hash"][id] = true
          if allIPs[id] then -- IP is associated with a current device
            ids[id] = allIPs[id]
          else -- See if the IP is in the AdGuard Home query log
            local querylog = json.decode(execute(string.format('curl -su "%s" "http://%s/control/querylog?limit=1&search=%s"',credentials,address,id)) or {})
            if #querylog.data > 0 then -- IP is in the query log
              ids[id] = hostname
            end
          end
        end
        client.ids = {} -- Empty the list of client IDs
        clients[hostname] = client -- Preserve all other settings
        delete[hostname] = true -- Mark it as to be deleted, until we know otherwise
        table.insert(names,hostname)
      end
    end
  end

  for devicename,ips in pairs(devIPs) do
    delete[devicename] = nil -- Mark it as NOT to be deleted
    table.insert(names,devicename)
    for _,id in pairs(ips) do
      ids[id] = devicename
    end
  end

  for id,hostname in pairs(ids) do
    local client = clients[hostname]
    if client then
      update[hostname] = not append[hostname]
    else
      append[hostname] = true
      client = {
        name = hostname,
        use_global_settings = true,
        filtering_enabled = true,
        parental_enabled = true,
        safebrowsing_enabled = true,
        safesearch_enabled = true,
        use_global_blocked_services = true,
        ids = {}
      }
      clients[hostname] = client
    end
    table.insert(client.ids,id)
  end

  names = deduplicate(names)

  for _,hostname in ipairs(names) do
    if delete[hostname] then
      print("Deleting AdGuard Client "..hostname.." from "..address)
      post(address,credentials,"delete",{
        name = hostname
      })
    end
  end
  for _,hostname in ipairs(names) do
    if append[hostname] then
      print("Inserting AdGuard Client "..hostname.." to "..address)
      table.sort(clients[hostname].ids)
      post(address,credentials,"add",clients[hostname])
    end
  end
  for _,hostname in ipairs(names) do
    if update[hostname] and not the_same(clientids[hostname],clients[hostname].ids) then
      print("Updating AdGuard Client "..hostname.." on "..address)
      table.sort(clients[hostname].ids)
      post(address,credentials,"update",{
        name = hostname,
        data = clients[hostname]
      })
    end
  end
end

for _,target in pairs(targets) do
  process(target)
end
