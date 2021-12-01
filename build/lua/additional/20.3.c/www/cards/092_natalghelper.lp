--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local toupper = string.upper

local ngx = ngx
local session = ngx.ctx.session
local modalPath = "/modals/nat-alg-helper-modal.lp"

if session:hasAccess(modalPath) then
  local helpers = {}
  -- Populated by 102-NAT-ALG
  local available = {}

  for _,v in pairs(proxy.getPN("uci.firewall_helpers.helper.",true)) do
    local helper = proxy.get(v.path.."name",v.path.."enable")
    if helper then
      local name = untaint(helper[1].value)
      if available[name] then
        helpers[#helpers+1] = { name = toupper(name),enable = untaint(helper[2].value) }
      end
    end
  end

  table.sort(helpers,function(a,b)
    return a.name < b.name
  end)

  local mid = math.floor((#helpers / 2) + 0.5)
  local attributes = { span = { class = "helper-card" } }
  local htmlLeft = {}
  local htmlRight = {}
  for _,v in pairs(helpers) do
    local state = v.enable
    local text
    if state ~= "0" then
      text =" enabled<br>"
      state = "1"
    else
      text = " disabled<br>"
    end
    if #htmlLeft < mid then
      htmlLeft[#htmlLeft+1] = ui_helper.createSimpleLight(state,v.name,attributes)..text
    else
      htmlRight[#htmlRight+1] = ui_helper.createSimpleLight(state,v.name,attributes)..text
    end
  end

  ngx.print('\
  <div class="span3">\
    <div class="smallcard">\
      '); ngx.print(ui_helper.createCardHeader(T"NAT Helpers",modalPath)); ngx.print('\
      <div class="content card_bg" data-bg-text="&#xf0ad;">\
        <div style="display:flex;flex-direction:row;">\
          <div style="padding-right:3px">\
            ');
            ngx.print(htmlLeft);
            ngx.print('\
          </div>\
          <div>\
            ');
            ngx.print(htmlRight);
            ngx.print('\
          </div>\
        </div>\
      </div>\
    </div>\
  </div>\
');
end
ngx.print('\
');
