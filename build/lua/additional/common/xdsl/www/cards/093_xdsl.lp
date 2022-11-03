--pretranslated: do not change this file

gettext.textdomain('webui-core')

local lfs = require("lfs")
local session = ngx.ctx.session
local modalPath = "/modals/xdsl-low-modal.lp"

if session:hasAccess(modalPath) and lfs.attributes("/etc/config/xdsl", "mode") == "file" then
  local visible = require("bridgedmode_helper").isBridgedMode()
  if not visible then
    local wan_ifname = require("datamodel").get("uci.network.interface.@wan.ifname")
    if wan_ifname then
      ---@diagnostic disable-next-line: undefined-field
      local v = string.untaint(wan_ifname[1].value)
      visible = v:find("ptm0") or v:find("atmwan")
    end
  end

  if visible then
    local content_helper = require("web.content_helper")
    local ui_helper = require("web.ui_helper")
    local format = string.format

    local content = {
      dsl0_enabled = "uci.xdsl.xdsl.@dsl0.enabled",
      line_status = "sys.class.xdsl.@line0.Status",
      xdsl_status = "rpc.xdsl.status",
      xdsl_version = "rpc.xdslctl.DslFirmware",
      dslam_chipset = "rpc.xdslctl.DslamChipset",
      dslam_version = "rpc.xdslctl.DslamVersion",
    }
    content_helper.getExactContent(content)

    local enabled = (content.dsl0_enabled and content.dsl0_enabled == "1") and "1" or "0"
    local line_status = (content.line_status and content.line_status == "Up") and "1" or "0"
    local xdsl_status = (content.xdsl_status and content.xdsl_status == "Idle") and "1" or "0"

    if not content.dslam_chipset then
      content.dslam_chipset = T"Not Up"
      content.dslam_version = " "
    else
      if content.dslam_chipset:match("BDCM") then
        content.dslam_chipset = "Broadcom"
      elseif content.dslam_chipset:match("IFTN") then
        content.dslam_chipset = "Infineon"
      end
    end

    local enabled_state_map = {
      ["0"] = T"xDSL Disabled",
      ["1"] = T"xDSL Enabled",
    }

    ngx.print('\
    <div class="span3">\
      <div class="smallcard">\
        ',ui_helper.createCardHeader(T"xDSL Config",modalPath),'\
          <div class="content card_bg" data-bg-text="&#xf0ad;">',
            ui_helper.createSimpleLight(enabled,enabled_state_map[enabled]),
            ui_helper.createSimpleLight(xdsl_status,format(T"xDSL Status: %s",content.xdsl_status)),
            ui_helper.createSimpleLight(line_status,format(T"Line Status: %s",content.line_status)),'\
            <p class="subinfos">',
              format(T"Datapump Version"..": <strong style='letter-spacing:-1px;font-size:12px;'>%s</strong>",content.xdsl_version),'\
              <br/>')
              if content.dslam_chipset ~= "" then
                ngx.print(
                  format(T"DSLAM Chipset: <strong style='letter-spacing:-1px;font-size:12px;'>%s</strong>",content.dslam_chipset),
                  '<br/>',
                  format(T"DSLAM Version: <strong style='letter-spacing:-1px;font-size:12px;'>%s</strong>",content.dslam_version)
                )
              end
              ngx.print('\
            </p>\
          </div>\
        </div>\
    </div>\
    ')
  end
end
