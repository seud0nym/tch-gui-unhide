local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local format = string.format
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local light_map = {
  disabled = "0",
  enabled = "1",
  running = "2",
  paused = "4",
}

local M = {}

function M.getAdblockStatus()
  local enabled = proxy.getPN("uci.adblock.global.adb_sources.", true) or {}
  local content = {
    state = "rpc.adblock.status",
    last_rundate = "rpc.adblock.last_rundate",
    blocked_domains = "rpc.adblock.blocked_domains",
    version = "rpc.adblock.version",
    custom_whitelist = "rpc.adblock.WhiteListNumberOfEntries",
    custom_blacklist = "rpc.adblock.BlackListNumberOfEntries",
  }
  content_helper.getExactContent(content)

  content.status_text = T("Ad blocking "..content.state)
  content.status = light_map[untaint(content.state)]
  content.enabled_lists = #enabled

  return content
end

function M.getAdblockCardHTML()
  local content = M.getAdblockStatus()
  local blocked = tonumber(content.blocked_domains) or 0
  local white = tonumber(content.custom_whitelist) or 0
  local black = tonumber(content.custom_blacklist) or 0
  local html = {}
  html[#html+1] = ui_helper.createSimpleLight(content.status,content.status_text)
  html[#html+1] = '<p class="subinfos">'
  html[#html+1] = format('<strong>Version</strong> %s',content.version)
  html[#html+1] = '<br>'
  html[#html+1] = format('<strong>Lists updated:</strong> %s',content.last_rundate)
  html[#html+1] = '<br>'
  html[#html+1] = format("<strong class='modal-link' data-toggle='modal' data-remote='/modals/adblck-sources-modal.lp' data-id='adblck-sources-modal'>%d DNS Block %s</strong> enabled",content.enabled_lists,N("List","Lists",content.enabled_lists))
  html[#html+1] = '<br>'
  html[#html+1] = format("<strong class='modal-link' data-toggle='modal' data-remote='/modals/adblck-lists-modal.lp' data-id='adblck-lists-modal'>%d Custom White List</strong> %s",white,N("domain","domains",white))
  html[#html+1] = '<br>'
  html[#html+1] = format("<strong class='modal-link' data-toggle='modal' data-remote='/modals/adblck-lists-modal.lp' data-id='adblck-lists-modal'>%d Custom Black List</strong> %s",black,N("domain","domains",black))
  html[#html+1] = '<br>'
  html[#html+1] = format("<strong class='modal-link' data-toggle='modal' data-remote='/modals/adblck-sources-modal.lp' data-id='adblck-sources-modal'>%d %s</strong> blocked",blocked,N("Domain","Domains",blocked))
  html[#html+1] = '</p>'
  return html
end

return M
