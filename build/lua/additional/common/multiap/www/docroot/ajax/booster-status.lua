local json = require("dkjson")
local booster_helper = require("booster_helper")
local content_helper = require("web.content_helper")

local content = {
  agent_enabled = "uci.multiap.agent.enabled",
  controller_enabled = "uci.multiap.controller.enabled",
}

content_helper.getExactContent(content)

local html = booster_helper.getBoosterCardHTML(content.agent_enabled,content.controller_enabled)

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
