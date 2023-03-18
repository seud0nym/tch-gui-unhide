local M = {}

-- QR-Code escape function for SSID and PSK strings
-- the following characters needs to be escaped with a backslash (\): backslash (\), single-quote ('), double-quote ("),
-- dot (.), colon (:), comma (,), and semicolon (;)
-- we have the lua and the JavaScript escaping to pass as well, that's way you see so many backslashes
local qrcode_escape_entities = {  ["\\"] = "\\\\", ["'"] = "\\'", ['"'] = '\\"',
                  ["."] = "\\.", [":"] = "\\:", [","] = "\\,", [";"] = "\\;" }
local javascript_escape_entities = {  ["\\"] = "\\\\", ["'"] = "\\'", ['"'] = '\\"'}

local function detailval(content, secmodes, keypassphrases, wifimode)
  local ssid, password, mode, public
  if (content.ssid) or (content.ssid24) or (content.ssid5) then
    if not wifimode then
      mode =  secmodes[content.secmode] or secmodes[content.secmode24]
      password = keypassphrases[content.secmode] or keypassphrases[content.secmode24]
      ssid = content.ssid or content.ssid24
      public = content.public or content.public24
    else
      mode =  secmodes[content.secmode5]
      password = keypassphrases[content.secmode5]
      ssid = content.ssid5
      public = content.public5
    end
  end
  return mode, password, ssid, public
end

--- Create QR code generation string
-- @param #table content table with fields: ssid, secmode, wpakey, public and wepkey
-- @param #table secmodes complete list of security modes available
-- @param #table keypassphrases passwords for different security modes
-- @param #bool wifimode true for 5GHz frequency else false
-- @return #string wlanconfstr QR code string
function M.format(content, secmodes, keypassphrases, wifimode)
  local wlanconfstr = ""
  local mode, password, ssid, public = detailval(content, secmodes, keypassphrases, wifimode)

  --Use WLAN configuratin to compose a string: "WIFI:T:WEP;S:mynetwork;P:mypassword;;"
  --These parameters are needed: SSID, Security Mode and password
  local hidden = ";H:false;;"
  if public == "0" then
    hidden = ";H:true;;"
  end

  --qr code escaping of dynamic data
  ssid = ssid:gsub(".", qrcode_escape_entities)
  password = password:gsub(".", qrcode_escape_entities)

  if mode == "nopass" then
    wlanconfstr  = "WIFI:T:" .. mode .. ";S:" .. ssid .. hidden
  else
    wlanconfstr  = "WIFI:T:" .. mode .. ";S:" .. ssid .. ";P:" .. password .. hidden
  end

  --javascript escaping of special characters
  wlanconfstr = wlanconfstr:gsub(".", javascript_escape_entities)

  return wlanconfstr
end

return M
