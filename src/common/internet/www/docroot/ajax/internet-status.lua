local json = require("dkjson")
local dyntab_helper = require("web.dyntab_helper")
local ich = require("internetcard_helper")
local imh = require("internetmode_helper")

local mode_active = dyntab_helper.process(imh).current.name
if mode_active == "" then
  for _,v in ipairs(imh) do
    if v.default == true then
      mode_active = v.name
      break
    end
  end
end

local html = ich.getInternetCardHTML(mode_active)

local data = {
  html = table.concat(html,"\n"),
}

local buffer = {}
if json.encode (data,{ indent = false,buffer = buffer }) then
  ngx.say(buffer)
else
  ngx.say("{}")
end
ngx.exit(ngx.HTTP_OK)
