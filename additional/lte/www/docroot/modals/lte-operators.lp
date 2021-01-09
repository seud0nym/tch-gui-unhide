--pretranslated: do not change this file
 
local string = string
local table = { insert = table.insert }
local ngx, type, format, istainted = ngx, type, string.format, string.istainted

-- Enable localization
gettext.textdomain('webui-mobiled')

local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local utils = require("web.lte-utils")

local params = utils.get_params()

local used_plmn = {}
local devices = proxy.get("uci.mobiled.numDevices")
if devices and tonumber(devices[1].value) > 0 then
  for i = 1,tonumber(devices[1].value),1 do
    local mcc = format("%s", proxy.get("uci.mobiled.device.@" .. i .. ".mcc")[1].value)
    local mnc = format("%s", proxy.get("uci.mobiled.device.@" .. i .. ".mnc")[1].value)
    used_plmn[mcc .. mnc] = 1
  end
end

local operator_valid = {
	["name"] = post_helper.getValidateStringLengthInRange(1, 127),
	["mcc"] =  post_helper.getAndValidation(post_helper.getValidateStringLengthInRange(3, 3), post_helper.validatePositiveNum),
	["mnc"] = post_helper.getAndValidation(post_helper.getValidateStringLengthInRange(2, 3), post_helper.validatePositiveNum),
}

local function operator_filter(data)
  if used_plmn[format("%s", data.plmn)] then
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
    attr = { input = { class="span2", maxlength="127" } }
	},
	{
		header = T"Mobile Country Code",
		name = "mcc",
		param = "mcc",
    type = "text",
    default = "505",
		attr = { input = { type="number", minlength="3", maxlength="3" } }
	},
	{
		header = T"Mobile Network Code",
		name = "mnc",
		param = "mnc",
		type = "text",
		attr = { input = { type="number", minlength="2", maxlength="3" } }
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
	createMsg = T"Add new operator",
	minEntries = 1
}

local operator_data, operator_helpmsg = post_helper.handleTableQuery(operator_columns, operator_options, operator_filter, nil, operator_valid)

  ngx.print('\
');  ngx.print( ui_helper.createHeader(params.modal_title, false, true) ); ngx.print('\
<div class="modal-body update" id="lte-body">\
	<form class="form-horizontal" method="post" action="modals/lte-operators.lp">\
		');  
			local lp = require("web.lp")
			lp.setpath("/www/snippets/")
			lp.include("tabs-mobiled.lp")

			local html = {}
			table.insert(html, "<fieldset><legend>" .. T"Operators" .. "</legend>")
			table.insert(html, ui_helper.createTable(operator_columns, operator_data, operator_options, nil, operator_helpmsg))
			table.insert(html, "</fieldset>")
			table.insert(html, "<a target='_blank' href='https://en.wikipedia.org/wiki/Mobile_Network_Codes_in_ITU_region_5xx_(Oceania)#Australia_%E2%80%93_AU'>Click here for valid MCC and MNC values</a>")
			ngx.print(html)
		  ngx.print('\
	</form>\
</div>\
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
'); 