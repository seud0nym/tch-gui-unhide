---@diagnostic disable: lowercase-global
json = require('dkjson')

script_name = "agh-clients.lua"
lock_dir = "/tmp/agh-clients.lock"

debug = false
for i = 1,#arg,1 do
  if arg[i] == "--debug" then
    debug = true
    table.remove(arg, i)
    break
  end
end

if (#arg % 2 ~= 0) then
  print("Required arguments: hostname:port username:password")
  print("                    Repeat hostname:port username:password for multiple servers")
  os.exit()
end

targets = {}
for i = 1, #arg, 2 do
  targets[#targets + 1] = {
    address = arg[i],
    credentials = arg[i + 1]
  }
end

function log(message)
  if not debug and string.find(message, "^DEBUG:") then
    return
  end
  print(os.date("%H:%M:%S") .. " " .. message)
end

function execute(cmd)
  log("DEBUG: Executing: " .. cmd)
  local popen = io.popen(cmd)
  if popen then
    local response = popen:read('*a')
    popen:close()
    return response
  end
  return nil
end

function http_get(address, credentials, endpoint)
  return json.decode(execute(string.format('curl -su "%s" http://%s/control/%s', credentials, address, endpoint)) or {})
end

function http_post_client(address, credentials, action, data)
  local json = json.encode(data)
  local result = execute(string.format('curl -su "%s" -X POST -H "Content-Type: application/json" --data \'%s\' http://%s/control/clients/%s', credentials, json, address, action))
  if result and #result > 0 then
    log("ERROR: " .. result)
  end
end

function ip_is_in_querylog(address, credentials, ip)
  local querylog = http_get(address, credentials, string.format("querylog?limit=1&search=%s", ip))
  if #querylog.data > 0 then
    return true
  end
  return false
end

function get_known_hosts(address, credentials)
  local l3interface = execute(string.format('ip route get fibmatch %s | cut -d" " -f3 | xargs echo -n', string.match(address, "([^:]+)"))) or "br-lan"
  local devIPs = {}
  local allIPs = {}

  log("INFO:  Getting known hosts on " .. l3interface)

  function add_ip_from(hostname, ips)
    for _, ip in pairs(ips) do
      if ip.state == "connected" then --or ip_is_in_querylog(address, credentials, ip.address) then
        table.insert(devIPs[hostname], ip.address)
        allIPs[ip.address] = hostname
      end
    end
  end

  local devices = json.decode(execute('ubus call hostmanager.device get'))
  table.sort(devices, function(a, b)
    return string.lower(a.hostname) < string.lower(b.hostname)
  end)

  for _, host in pairs(devices) do
    if host.l3interface == l3interface then
      local hostname = host.hostname
      if not hostname or hostname == "" then
        hostname = host['user-friendly-name']
        if not hostname or hostname == "" then
          hostname = string.format("Unknown-%s", host['mac-address'])
        end
      end
      local last2char = string.sub(hostname, -2)
      if last2char == '-2' or last2char == '-3' or last2char == '-4' or last2char == '-5' then
        hostname = string.sub(hostname, 1, -3)
      end
      devIPs[hostname] = {}
      add_ip_from(hostname, host.ipv4)
      add_ip_from(hostname, host.ipv6)
    end
  end

  local localhost = execute('echo -n $HOSTNAME') or "mymodem"
  devIPs[localhost] = {}
  for cidr in string.gmatch(execute(string.format('ip -o -br address show %s | tr -s " " | cut -d" " -f3-', l3interface)), "([^%s]+)") do
    local ip = string.match(cidr, "([^/]+)/%d+")
    log("DEBUG: " .. ip)
    table.insert(devIPs[localhost], ip)
    allIPs[ip] = localhost
  end

  return devIPs, allIPs
end

function deduplicate(t)
  local duplicate = {}
  local result = {}

  for _, v in pairs(t) do
    if not duplicate[v] then
      result[#result + 1] = v
      duplicate[v] = true
    end
  end

  table.sort(result, function(a, b)
    return string.lower(a) < string.lower(b)
  end)

  return result
end

function the_same(original, updated, hostname)
  if original.count ~= #updated then
    log(string.format("DEBUG: %s ID original.count ~= #updated (%s ~= %s)", hostname, original.count, #updated))
    return false
  end
  for _, v in pairs(updated) do
    if not original.hash[v] then
      log("DEBUG: Updated " .. hostname .. " ID " .. v .. " not in original.hash")
      return false
    end
  end
  log("DEBUG: " .. hostname .. " IDs unchanged")
  return true
end

function process(target)
  local address = target.address
  local credentials = target.credentials
  local devIPs, allIPs = get_known_hosts(address, credentials)

  log("INFO:  Updating AdGuard Home on " .. address)

  local append = {}
  local delete = {}
  local update = {}

  local clients = {}
  local ids = {}
  local names = {}

  local clientids = {}

  local current = http_get(address, credentials, 'clients')

  if current.clients then
    table.sort(current.clients, function(a, b)
      return string.lower(a.name) < string.lower(b.name)
    end)
    for _, client in pairs(current.clients) do
      if client.ids then
        local hostname = client.name
        log("DEBUG: Found client " .. hostname .. " with " .. #client.ids .. " IDs")
        clientids[hostname] = {
          count = #client.ids,
          hash = {},
        }
        for n, id in ipairs(client.ids) do
          clientids[hostname]["hash"][id] = true
          if allIPs[id] then -- IP is associated with a current device
            log("DEBUG: Found client " .. hostname .. " ID #" .. n .. " " .. id .. " associated with device " .. allIPs[id])
            ids[id] = allIPs[id]
          else -- See if the IP is in the AdGuard Home query log
            if ip_is_in_querylog(address, credentials, id) then
              log("DEBUG: Found client " .. hostname .. " ID #" .. n .. " " .. id .. " in query log")
              ids[id] = hostname
            end
          end
        end
        clients[hostname] = client -- Preserve all other settings
        clients[hostname].ids = {} -- Empty the ID list
        delete[hostname] = true    -- Mark it as to be deleted, until we know otherwise
        table.insert(names, hostname)
      end
    end
  else
    log("DEBUG: No current AdGuard Clients found on " .. address)
  end

  for hostname, ips in pairs(devIPs) do
    delete[hostname] = nil -- Mark it as NOT to be deleted
    table.insert(names, hostname)
    for _, id in pairs(ips) do
      ids[id] = hostname
    end
  end

  for id, hostname in pairs(ids) do
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
    log("DEBUG: Updated client " .. hostname .. " with ID " .. id)
    table.insert(client.ids, id)
  end

  names = deduplicate(names)

  for _, hostname in ipairs(names) do
    if delete[hostname] then
      log("INFO:  Deleting client " .. hostname .. " from " .. address)
      http_post_client(address, credentials, "delete", {name = hostname})
    end
  end
  for _, hostname in ipairs(names) do
    if append[hostname] then
      log("INFO:  Inserting client " .. hostname .. " to " .. address)
      table.sort(clients[hostname].ids)
      http_post_client(address, credentials, "add", clients[hostname])
    end
  end
  for _, hostname in ipairs(names) do
    if update[hostname] and not the_same(clientids[hostname], clients[hostname].ids, hostname) then
      log("INFO:  Updating client " .. hostname .. " on " .. address)
      table.sort(clients[hostname].ids)
      http_post_client(address, credentials, "update", {
        name = hostname,
        data = clients[hostname]
      })
    end
  end
end

if os.execute("mkdir " .. lock_dir) ~= 0 then
  print("WARN:  Another instance is already running (via lock directory).")
  -- Check if another instance is running
  local handle = io.popen("pgrep -f " .. script_name .. " | wc -l")
  if not handle then
    print("ERROR: Unable to check for running instances.")
    os.exit(1)
  end
  local count = tonumber(handle:read("*a"))
  handle:close()
  if count > 1 then
    print("ERROR: Another instance is already running (via process check).")
    os.exit(1)
  end
end

local function cleanup()
  os.execute("rmdir " .. lock_dir)
end
-- Register cleanup on exit
pcall(function()
  for _, target in pairs(targets) do
    process(target)
  end
end)
cleanup()




