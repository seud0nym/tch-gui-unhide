local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local untaint = string.untaint

local light_map = {
  disabled = "0",
  enabled = "1",
  running = "2",
  paused = "4",
}

local M = {}

function M.getAdblockStatus()
  local content = {
    status = "rpc.gui.adblock.status",
    last_rundate = "rpc.gui.adblock.last_rundate",
    overall_domains = "rpc.gui.adblock.overall_domains",
  }
  content_helper.getExactContent(content)

  return light_map[untaint(content.status)], T("Ad blocking " .. content.status), untaint(content.overall_domains), untaint(content.last_rundate)
end

function M.getAdblockCardHTML()
  local status,status_text,overall_domains,last_rundate = M.getAdblockStatus()
  local html = {}
  html[#html+1] = ui_helper.createSimpleLight(status,status_text)
  html[#html+1] = '<p class="subinfos">'
  html[#html+1] = 'Domains: ' .. overall_domains
  html[#html+1] = '<br>'
  html[#html+1] = 'Last Updated: ' .. last_rundate
  html[#html+1] = '</p>'
  return html
end

return M
