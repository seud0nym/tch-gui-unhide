--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local ngx = ngx
local session = ngx.ctx.session
local modalPath = "/modals/nat-alg-helper-modal.lp"

if session:hasAccess(modalPath) then
  local proxy = require("datamodel")
  local ui_helper = require("web.ui_helper")
  local alg_modal_link = "class=\"modal-link\" data-toggle=\"modal\" data-remote=\"/modals/nat-alg-helper-modal.lp\" data-id=\"nat-alg-helper-modal\""
  local format, toupper, untaint = string.format, string.upper, string.untaint
  local html = {}

  local zones = proxy.getPN("uci.firewall.zone.", true)
  for k,v in ipairs(zones) do
    local name = proxy.get(v.path .. "name")
    local wan = proxy.get(v.path .. "wan")
    if name and (name[1].value == "lan" or (wan and wan[1].value == "1")) then
      local helpers = proxy.get(v.path .. "helper.")
      if helpers then
        local light = "0"
        if #helpers > 0 then
          light = "1"
        end
        html[#html+1] = ui_helper.createSimpleLight(light, format(N("<strong %1$s>%2$d %s NAT ALG Helper</strong> enabled","<strong %1$s>%2$d %s NAT ALG Helpers</strong> enabled", #helpers), alg_modal_link, #helpers, toupper(untaint(name[1].value))))
      end
    end
  end

  if #html > 0 then
    ngx.print('\
  <div class="span3">\
    <div class="smallcard">\
      ');  ngx.print( ui_helper.createCardHeader(T"NAT Helpers", modalPath) ); ngx.print('\
      <div class="content card_bg" data-bg-text="&#xf0ec;">\
        ');  
        ngx.print(html)
        ngx.print('\
      </div>\
    </div>\
  </div>\
  ');  
  end
end
  ngx.print('\
'); 
