--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local sub = string.sub

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local shaper_columns = {
  {
    header = T"Shaper Alias",
    name = "Alias",
    param = "Alias",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Shaping Rate",
    name = "ShapingRate",
    param = "ShapingRate",
    type = "text",
    attr = { input = { type = "number",min = "-1",step = "1000",class = "span2" } },
  },
  {
    header = T"Shaping Burst Size",
    name = "ShapingBurstSize",
    param = "ShapingBurstSize",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Interface",
    name = "Interface",
    param = "Interface",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Status",
    name = "Status",
    param = "Status",
    type = "text",
    readonly = true,
  },
}

local shaper_options = {
  tableid = "shaper",
  basepath = "Device.QoS.Shaper.",
  canEdit = false,
  canAdd = false,
  canDelete = false,
}

local shaper_valid = {
}

local shaper_data,shaper_helpMsg = post_helper.handleTableQuery(shaper_columns,shaper_options,nil,nil,shaper_valid)

for r,row in ipairs(shaper_data) do
  for c,col in ipairs(row) do
    if col == "-1" then
      shaper_data[r][c] = ""
    else
      local value = untaint(col)
      if sub(value,1,7) == "Device." then
        local name = proxy.get(value..".Name")
        if name then
          shaper_data[r][c] = name[1].value
        end
      end
    end
  end
end

ngx.print(ui_helper.createHeader(T"QoS",false)); ngx.print('\
<div class="modal-body">\
'); lp.include("tabs-qos.lp") ngx.print('\
  <form class="form-horizontal" method="post" action="modals/qos-swshaper-modal.lp">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>Shapers</legend>\
      ',ui_helper.createTable(shaper_columns,shaper_data,shaper_options,nil,shaper_helpMsg),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter())
