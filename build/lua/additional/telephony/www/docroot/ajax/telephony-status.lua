local json = require("dkjson")
local datamodel = require ("datamodel")
local tch = require("telephonycard_helper")

local mmpbx_state = datamodel.get("uci.mmpbx.mmpbx.@global.enabled")
if mmpbx_state then
  mmpbx_state = mmpbx_state[1].value
else
  mmpbx_state = "0"
end

local html = tch.getTelephonyCardHTML(mmpbx_state)

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
