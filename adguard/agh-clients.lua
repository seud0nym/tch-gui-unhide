---@diagnostic disable: lowercase-global
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

json=require('dkjson')

cmd = io.popen('ubus call hostmanager.device get')
raw = cmd:read('*a')
cmd:close()
hosts = json.decode(raw)

device_ips = {}
cmd = io.popen('echo -n $HOSTNAME')
device_hostname = cmd:read('*a')
cmd:close()
cmd = io.popen('ip -o -br a | grep br-lan | tr -s " " | cut -d" " -f3-')
device_ips = cmd:read('*a')
cmd:close()

function process(target)
  local address = target.address
  local credentials = target.credentials
  
  local append = {}
  local delete = {}
  local update = {}

  local clients = {}
  local ids = {}
  local index = {}

  local cmd = string.format('curl -su "%s" http://%s/control/clients', credentials, address)
  local curl = io.popen(cmd)
  local raw = curl:read('*a')
  curl:close()
  local current = json.decode(raw)

  if current.clients then
    for i,client in pairs(current.clients) do
      if client.ids then
        delete[client.name] = true
        local hostname
        if string.sub(client.name,-2) == '-2' then
          hostname = string.sub(client.name,1,-3)
        else
          hostname = client.name
        end
        index[hostname] = {
          source = i,
        }
        if not clients[hostname] then
          clients[hostname] = {
            ids = {}
          }
        end
        for _,id in pairs(client.ids) do
          clients[hostname].ids[#clients[hostname].ids+1] = id
          ids[id] = hostname
        end
      end
    end
  end
  
  delete[device_hostname] = false
  if not clients[device_hostname] then
    clients[device_hostname] = {
      ids = {}
    }
  end
  for ip in string.gmatch(device_ips, "(%S+)/%d%d") do
    clients[device_hostname].ids[#clients[device_hostname].ids+1] = ip
    ids[ip] = device_hostname
  end

  for _,host in pairs(hosts) do
    if host.interface == "lan" then
      local mac = host['mac-address']
      local hostname = host.hostname
      if not hostname or hostname == "" then
        hostname = host['user-friendly-name']
        if not hostname or hostname == "" then
          hostname = string.format("Unknown-%s", mac)
        end
      end
      if string.sub(hostname,-2) == '-2' then
        hostname = string.sub(hostname,1,-3)
      end
      delete[hostname] = false
      ids[mac] = hostname
      for _,ipv4 in pairs(host.ipv4) do
        ids[ipv4.address] = hostname
      end
      for _,ipv6 in pairs(host.ipv6) do
        ids[ipv6.address] = hostname
      end
    end
  end

  for id, hostname in pairs(ids) do
    local i = index[hostname]
    if not i then
      append[#append+1] = {
        name = hostname,
        use_global_settings = true,
        filtering_enabled = true,
        parental_enabled = true,
        safebrowsing_enabled = true,
        safesearch_enabled = true,
        use_global_blocked_services = true,
        ids = {}
      }
      index[hostname] = {
        target = "A",
        index = #append
      }
    elseif not i.target then 
      update[#update+1] = {
        name = hostname,
        data = {
          use_global_settings = true,
          filtering_enabled = true,
          parental_enabled = true,
          safebrowsing_enabled = true,
          safesearch_enabled = true,
          use_global_blocked_services = true,
          name = hostname,
          ids = {}
        }
      }    
      index[hostname] = {
        target = "U",
        index = #update
      }
    end
    if index[hostname].target == "A" then
      append[index[hostname].index].ids[#append[index[hostname].index].ids+1] = id
    else
      update[index[hostname].index].data.ids[#update[index[hostname].index].data.ids+1] = id
    end
  end

  for host,v in pairs(delete) do
    if v then
      print("Removing client "..host.." from "..address)
      data = json.encode({ name = host})
      cmd = string.format('curl -u "%s" -X POST --data \'%s\' http://%s/control/clients/delete', credentials, data, address)
      os.execute(cmd)
    end
  end
  for _,host in pairs(append) do
    print("Adding client "..host.name.." to "..address)
    data = json.encode(host)
    cmd = string.format('curl -u "%s" -X POST --data \'%s\' http://%s/control/clients/add', credentials, data, address)
    os.execute(cmd)
  end
  for _,host in pairs(update) do
    print("Updating client "..host.name.." on "..address)
    data = json.encode(host)
    cmd = string.format('curl -u "%s" -X POST --data \'%s\' http://%s/control/clients/update', credentials, data, address)
    os.execute(cmd)
  end
end

for _,target in pairs(targets) do
  process(target)
end
