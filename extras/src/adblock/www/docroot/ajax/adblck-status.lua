local json = require("dkjson")
local adblock_helper = require("adblock_helper")
local html = adblock_helper.getAdblockCardHTML()

local data = {
  html = table.concat(html, "\n"),
}

local buffer = {}
if json.encode (data, { indent = false, buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
