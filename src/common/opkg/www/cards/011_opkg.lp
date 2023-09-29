--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local format = string.format
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")

local opkg_data = {
  customfeeds = "rpc.gui.opkg.customfeeds",
  last_update = "rpc.gui.opkg.last_update",
  system_installed = "rpc.gui.opkg.systemNumberOfEntries",
  user_installed = "rpc.gui.opkg.userNumberOfEntries",
  user_upgradable = "rpc.gui.opkg.user_upgradable",
}
content_helper.getExactContent(opkg_data)

local user_installed = tonumber(opkg_data["user_installed"])

local opkg_text = {}
if opkg_data["customfeeds"] ~= "" then
  opkg_text["configured"] = T"Custom feeds configured"
  opkg_data["configured"] = T"1"
else
  opkg_text["configured"] = T"No Custom feeds configured"
  opkg_data["configured"] = T"0"
end
if opkg_data["last_update"] == "-1" then
  opkg_text["last_update"] = T"Not since reboot"
else
  opkg_text["last_update"] = T(os.date("%d/%m/%Y %H:%M:%S",tonumber(opkg_data["last_update"])))
end
opkg_text["system_installed"] = format("<strong class='modal-link' data-toggle='modal' data-remote='/modals/opkg-system-installed-modal.lp' data-id='opkg-sys-modal'>%d packages</strong> system installed",tonumber(opkg_data["system_installed"]))
opkg_text["user_installed"] = format(N("%s%d package%s","%s%d packages%s",user_installed),"<strong class='modal-link' data-toggle='modal' data-remote='/modals/opkg-user-installed-modal.lp' data-id='opkg-usr-modal'>",user_installed,"</strong> user installed")

local session = ngx.ctx.session
local modalPath
if session:hasAccess("/modals/opkg-modal.lp") then
  modalPath = "modals/opkg-modal.lp"
end

ngx.print('\
<div class="span3">\
  <div class="smallcard">',
    ui_helper.createCardHeader(T"Packages",modalPath,nil,nil,{icon={class="icon-gift"}}),'\
    <div class="content card_bg mirror" data-bg-text="&#xf06b;">',
      ui_helper.createSimpleLight(opkg_data["configured"],opkg_text["configured"]))
      if opkg_data["configured"] ~= "0" then
        ngx.print('\
          <span class="simple-desc"><i class="icon-calendar status-icon"></i> Last Update: <strong>',opkg_text["last_update"],'</strong></span>\
        ')
      end
      ngx.print('\
        <span class="simple-desc"><i class="icon-shield status-icon"></i> ',opkg_text["system_installed"],'</span>\
        <span class="simple-desc"><i class="icon-user status-icon"></i> ',opkg_text["user_installed"],'</span>\
      ')
      if opkg_data["user_upgradable"] ~= "0" then
        local user_upgradable = tonumber(opkg_data["user_upgradable"])
        ngx.print('\
          <span class="simple-desc"><i class="icon-circle-arrow-up orange"></i> <strong>',
          format(N("%d user package","%d user packages",user_upgradable),user_upgradable),
          ' upgradable</strong></span>\
        ')
      end
      ngx.print('\
    </div>\
  </div>\
</div>\
');