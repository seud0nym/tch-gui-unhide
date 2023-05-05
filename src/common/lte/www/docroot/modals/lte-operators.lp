--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-mobiled')

local lp = require("web.lp")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local utils = require("web.lte-utils")

local ngx,format = ngx,string.format

local params = utils.get_params()

local used_plmn
local serving_system = utils.getContent("rpc.mobiled.device.@1.network.serving_system.")
if serving_system and serving_system.mcc and serving_system.mnc then
  used_plmn = format("%s%s",serving_system.mcc,serving_system.mnc)
end

local operator_valid = {
  ["name"] = post_helper.getValidateStringLengthInRange(1,127),
  ["mcc"] =  post_helper.getAndValidation(post_helper.getValidateStringLengthInRange(3,3),post_helper.validatePositiveNum),
  ["mnc"] = post_helper.getAndValidation(post_helper.getValidateStringLengthInRange(2,3),post_helper.validatePositiveNum),
}

local function operator_filter(data)
  if used_plmn == data.plmn then
    data["used"] = T"Yes";
    return { canEdit = false, canDelete = false }
  else
    data["used"] = T"No";
    return { canEdit = true, canDelete = true }
  end
end

local operator_columns = {
  {
    header = T"Operator Name",
    name = "name",
    param = "name",
    type = "text",
    attr = { input = { class="span2",maxlength="127" } }
  },
  {
    header = T"Mobile Country Code",
    name = "mcc",
    param = "mcc",
    type = "text",
    default = "505",
    attr = { input = { type="number",minlength="3",maxlength="3" } }
  },
  {
    header = T"Mobile Network Code",
    name = "mnc",
    param = "mnc",
    type = "text",
    attr = { input = { type="number",minlength="2",maxlength="3" } }
  },
  {
    header = T"In Use",
    name = "used",
    param = "used",
    type = "text",
    readonly = true,
  }
}

local operator_options = {
  tableid = "lte_operators",
  basepath = "uci.mobiled.operator.@.",
  canAdd = true,
  canDelete = true,
  createMsg = T"Add Operator",
  minEntries = 1,
  sorted = "name",
}

local operator_data,operator_helpmsg = post_helper.handleTableQuery(operator_columns,operator_options,operator_filter,nil,operator_valid)

lp.setpath("/www/snippets/")

ngx.print(ui_helper.createHeader(params.modal_title,false,true),'\
<div class="modal-body update" id="lte-body">\
  <form class="form-horizontal" method="post" action="modals/lte-operators.lp">\
    '); lp.include("tabs-mobiled.lp"); ngx.print('\
    <fieldset><legend>',T"Operators",'</legend>',
      ui_helper.createTable(operator_columns,operator_data,operator_options,nil,operator_helpmsg),'\
    </fieldset>\
    <a target="_blank" href="https://en.wikipedia.org/wiki/Mobile_Network_Codes_in_ITU_region_5xx_(Oceania)">Click here for valid MCC and MNC values</a>\
  </form>\
</div>\
',ui_helper.createFooter())