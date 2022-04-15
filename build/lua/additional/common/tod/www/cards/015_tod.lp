--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
if session:hasAccess("/modals/tod-modal.lp") then
  local proxy = require("datamodel")
  local ui_helper = require("web.ui_helper")
  local content_helper = require("web.content_helper")
  ---@diagnostic disable-next-line: undefined-field
  local untaint = string.untaint
  local match,format = string.match,string.format

  local tod_paths = {
    tod = "uci.tod.hostNumberOfEntries",
    wifi = "uci.tod.wifitodNumberOfEntries",
  }

  content_helper.getExactContent(tod_paths)

  local rulecount = tod_paths.tod or 0
  local rulecount_wifi = tod_paths.wifi or 0

  if rulecount_wifi > 0 then
    for _,action in ipairs(proxy.getPN("uci.tod.action.",true)) do
      local path = action.path
      local enabled = proxy.get(path.."enabled")
      if enabled and enabled[1].value == "1" then
        local object = proxy.get(path.."object")
        if object and match(untaint(object[1].value),"wifitod.*") then
          local timer = proxy.get(path.."timers.@1.value")
          if timer and timer[1].value == "" then
            rulecount_wifi = rulecount_wifi - 1
          end
        end
      end
    end
  end

  ngx.print('\
  <div class="span3">\
    <div class="smallcard">\
      ',ui_helper.createCardHeader(T"Time of Day","/modals/tod-modal.lp"),'\
      <div class="content card_bg" data-bg-text="&#xf017;">\
        <div class="divtable">\
          <span class="simple-desc">',
            '<i class="icon-lock status-icon"></i>',
            format(N("<strong %s>%d Access Control</strong> rule defined",
                    "<strong %s>%d Access Control</strong> rules defined",rulecount),
                  'class="modal-link" data-toggle="modal" data-remote="/modals/tod-modal.lp" data-id="tod-modal"',
                  rulecount),
          '</span>')
          if session:hasAccess("/modals/tod_wireless-modal.lp") then
            ngx.print(
              '<span class="simple-desc">',
                '<i class="icon-lock status-icon"></i>',
                format(N("<strong %s>%d Wireless Control</strong> rule defined",
                        "<strong %s>%d Wireless Control</strong> rules defined",rulecount_wifi),
                      'class="modal-link" data-toggle="modal" data-remote="/modals/tod_wireless-modal.lp?tod" data-id="tod-modal"',
                      rulecount_wifi),
              '</span>')
          end
          ngx.print('\
        </div>\
      </div>\
    </div>\
  </div>\
  ')
end
