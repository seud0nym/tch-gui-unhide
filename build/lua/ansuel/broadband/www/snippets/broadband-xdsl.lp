--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ajax_helper = require("web.ajax_helper")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local format = string.format
local floor = math.floor
local tonumber = tonumber

local function delay2mode(delay)
  if not delay then
    return ""
  elseif delay > 2 then
    return "Interleaved"
  else
    return "Fast"
  end
end

local content = {
  dsl_status = "sys.class.xdsl.@line0.Status",
  dsl_type = "sys.class.xdsl.@line0.ModulationType",
  dsl_linerate_up = "sys.class.xdsl.@line0.UpstreamCurrRate",
  dsl_linerate_down = "sys.class.xdsl.@line0.DownstreamCurrRate",
  dsl_linerate_up_max = "sys.class.xdsl.@line0.UpstreamMaxRate",
  dsl_linerate_down_max = "sys.class.xdsl.@line0.DownstreamMaxRate",
  dsl_uptime = "sys.class.xdsl.@line0.ShowtimeStart",
  dsl_power_up = "sys.class.xdsl.@line0.UpstreamPower",
  dsl_power_down = "sys.class.xdsl.@line0.DownstreamPower",
  dsl_attenuation_up = "sys.class.xdsl.@line0.UpstreamAttenuation",
  dsl_attenuation_down = "sys.class.xdsl.@line0.DownstreamAttenuation",
  dsl_margin_up = "sys.class.xdsl.@line0.UpstreamNoiseMargin",
  dsl_margin_down = "sys.class.xdsl.@line0.DownstreamNoiseMargin",
  dsl_transfered_up = "sys.class.xdsl.@line0.BytesSent",
  dsl_transfered_down = "sys.class.xdsl.@line0.BytesReceived",
  dsl_downstream_delay = "sys.class.xdsl.@line0.DownstreamDelay",
  dsl_bonding_support = "sys.class.xdsl.@line0.BondingSupport",
  dsl_bonding_status = "sys.class.xdsl.@line0.BondingStatus"
}

local content_other_line = {
  dsl_status = "sys.class.xdsl.@line1.Status",
  dsl_type = "sys.class.xdsl.@line1.ModulationType",
  dsl_linerate_up = "sys.class.xdsl.@line1.UpstreamCurrRate",
  dsl_linerate_down = "sys.class.xdsl.@line1.DownstreamCurrRate",
  dsl_linerate_up_max = "sys.class.xdsl.@line1.UpstreamMaxRate",
  dsl_linerate_down_max = "sys.class.xdsl.@line1.DownstreamMaxRate",
  dsl_uptime = "sys.class.xdsl.@line1.ShowtimeStart",
  dsl_power_up = "sys.class.xdsl.@line1.UpstreamPower",
  dsl_power_down = "sys.class.xdsl.@line1.DownstreamPower",
  dsl_attenuation_up = "sys.class.xdsl.@line1.UpstreamAttenuation",
  dsl_attenuation_down = "sys.class.xdsl.@line1.DownstreamAttenuation",
  dsl_margin_up = "sys.class.xdsl.@line1.UpstreamNoiseMargin",
  dsl_margin_down = "sys.class.xdsl.@line1.DownstreamNoiseMargin",
  dsl_transfered_up = "sys.class.xdsl.@line1.BytesSent",
  dsl_transfered_down = "sys.class.xdsl.@line1.BytesReceived",
  dsl_downstream_delay = "sys.class.xdsl.@line1.DownstreamDelay"
}

content_helper.getExactContent(content)
content_helper.getExactContent(content_other_line)

local is_up = content["dsl_status"] == "Up" or content_other_line["dsl_status"] == "Up"
local dsl_mode = delay2mode(tonumber(content["dsl_downstream_delay"]))
local dsl_mode_other_line = delay2mode(tonumber(content_other_line["dsl_downstream_delay"]))
local dsl_max_linerate,dsl_linerate,dsl_transfered,dsl_power,dsl_attenuation,dsl_margin = "","","","","",""

if is_up then
  local iconUp = "<i class=\"icon-upload icon-large gray\"></i> "
  local iconDown = " <i class=\"icon-download icon-large gray\"></i> "

  local showBondingStats
  local bstats = tonumber(content["dsl_bonding_support"])
  showBondingStats = bstats == 1 and true or false

  local dslLineRateMaxUp,dslLineRateMaxDown = 0,0
  local otherDslLineRateMaxUp,otherDslLineRateMaxDown = 0,0
  if tonumber(content["dsl_linerate_up_max"]) and tonumber(content["dsl_linerate_down_max"]) then
    dslLineRateMaxUp = floor(content["dsl_linerate_up_max"] / 10) / 100
    dslLineRateMaxDown = floor(content["dsl_linerate_down_max"] / 10) / 100
  end
  if tonumber(content_other_line["dsl_linerate_up_max"]) and tonumber(content_other_line["dsl_linerate_down_max"]) then
    otherDslLineRateMaxUp = floor(content_other_line["dsl_linerate_up_max"] / 10) / 100
    otherDslLineRateMaxDown = floor(content_other_line["dsl_linerate_down_max"] / 10) / 100
  end
  if showBondingStats then
    dsl_max_linerate = format("%s %s Mbps %s %s Mbps | %s %s Mbps %s %s Mbps",iconUp,dslLineRateMaxUp,iconDown,dslLineRateMaxDown,iconUp,otherDslLineRateMaxUp,iconDown,otherDslLineRateMaxDown)
  else
    dsl_max_linerate = format("%s %s Mbps %s %s Mbps",iconUp,dslLineRateMaxUp,iconDown,dslLineRateMaxDown)
  end

  local dslLineRateUp,dslLineRateDown,otherDslLineRateUp,otherDslLineRateDown = 0,0,0,0
  if tonumber(content["dsl_linerate_up"]) and tonumber(content["dsl_linerate_down"]) then
    dslLineRateUp = floor(content["dsl_linerate_up"] / 10) / 100
    dslLineRateDown = floor(content["dsl_linerate_down"] / 10) / 100
  end
  if tonumber(content_other_line["dsl_linerate_up"]) and tonumber(content_other_line["dsl_linerate_down"]) then
    otherDslLineRateUp = floor(content_other_line["dsl_linerate_up"] / 10) / 100
    otherDslLineRateDown = floor(content_other_line["dsl_linerate_down"] / 10) / 100
  end
  if showBondingStats then
    dsl_linerate = format("%s %s Mbps %s %s Mbps | %s %s Mbps %s %s Mbps",iconUp,dslLineRateUp,iconDown,dslLineRateDown,iconUp,otherDslLineRateUp,iconDown,otherDslLineRateDown)
  else
    dsl_linerate = format("%s %s Mbps %s %s Mbps",iconUp,dslLineRateUp,iconDown,dslLineRateDown)
  end

  local dslTransferedUp,dslTransferedDown,otherDslTransferedUp,otherDslTransferedDown = 0,0,0,0
  if tonumber(content["dsl_transfered_up"]) and tonumber(content["dsl_transfered_down"]) then
    dslTransferedUp = floor(content["dsl_transfered_up"] / 1048576 * 100) / 100
    dslTransferedDown = floor(content["dsl_transfered_down"] / 1048576 * 100) / 100
  end
  if tonumber(content_other_line["dsl_transfered_up"]) and tonumber(content_other_line["dsl_transfered_down"]) then
    otherDslTransferedUp = floor(content_other_line["dsl_transfered_up"] / 1048576 * 100) / 100
    otherDslTransferedDown = floor(content_other_line["dsl_transfered_down"] / 1048576 * 100) / 100
  end
  if showBondingStats then
    dsl_transfered = format("%s %s MBytes %s %s MBytes | %s %s MBytes %s %s MBytes",iconUp,dslTransferedUp,iconDown,dslTransferedDown,iconUp,otherDslTransferedUp,iconDown,otherDslTransferedDown)
  else
    dsl_transfered = format("%s %s MBytes %s %s MBytes",iconUp,dslTransferedUp,iconDown,dslTransferedDown)
  end

  local dslPowerUp,dslPowerDown,otherDslPowerUp,otherDslPowerDown = "0","0","0","0"
  if tonumber(content["dsl_power_up"]) and tonumber(content["dsl_power_down"]) then
    dslPowerUp = content["dsl_power_up"]
    dslPowerDown = content["dsl_power_down"]
  end
  if tonumber(content_other_line["dsl_power_up"]) and tonumber(content_other_line["dsl_power_down"]) then
    otherDslPowerUp = content_other_line["dsl_power_up"]
    otherDslPowerDown = content_other_line["dsl_power_down"]
  end
  if showBondingStats then
    dsl_power = format("%s %s dBm %s %s dBm | %s %s dBm %s %s dBm",iconUp,dslPowerUp,iconDown,dslPowerDown,iconUp,otherDslPowerUp,iconDown,otherDslPowerDown)
  else
    dsl_power = format("%s %s dBm %s %s dBm",iconUp,dslPowerUp,iconDown,dslPowerDown)
  end

  local dslAttenuationUp,dslAttenuationDown,otherDslAttenuationUp,otherDslAttenuationDown
  dslAttenuationUp = content["dsl_attenuation_up"]
  dslAttenuationDown = content["dsl_attenuation_down"]
  otherDslAttenuationUp = content_other_line["dsl_attenuation_up"]
  otherDslAttenuationDown = content_other_line["dsl_attenuation_down"]
  if showBondingStats then
    dsl_attenuation = format("%s %s dB %s %s dB | %s %s dB %s %s dB",iconUp,dslAttenuationUp,iconDown,dslAttenuationDown,iconUp,otherDslAttenuationUp,iconDown,otherDslAttenuationDown)
  else
    dsl_attenuation = format("%s %s dB %s %s dB",iconUp,dslAttenuationUp,iconDown,dslAttenuationDown)
  end

  local dslMarginUp,dslMarginDown,otherDslMarginUp,otherDslMarginDown = "0","0","0","0"
  if tonumber(content["dsl_margin_up"]) and tonumber(content["dsl_margin_down"]) then
    dslMarginUp = content["dsl_margin_up"]
    dslMarginDown = content["dsl_margin_down"]
  end
  if tonumber(content_other_line["dsl_margin_up"]) and tonumber(content_other_line["dsl_margin_down"]) then
    otherDslMarginUp = content_other_line["dsl_margin_up"]
    otherDslMarginDown = content_other_line["dsl_margin_down"]
  end
  if showBondingStats then
    dsl_margin = format("%s %s dB %s %s dB | %s %s dB %s %s dB",iconUp,dslMarginUp,iconDown,dslMarginDown,iconUp,otherDslMarginUp,iconDown,otherDslMarginDown)
  else
    dsl_margin = format("%s %s dB %s %s dB",iconUp,dslMarginUp,iconDown,dslMarginDown)
  end
end

ajax_helper.handleAjaxQuery({dsl_uptime = "sys.class.xdsl.@line0.ShowtimeStart"},function(c)
  c.dsl_uptime = post_helper.secondsToTime(c["dsl_uptime"])
  c.dsl_max_linerate = dsl_max_linerate
  c.dsl_linerate = dsl_linerate
  c.dsl_transfered = dsl_transfered
  c.dsl_power = dsl_power
  c.dsl_attenuation = dsl_attenuation
  c.dsl_margin = dsl_margin
end)

local dsl_switch_params = {
  dsl_enable = "sys.class.xdsl.@line0.Enable"
}
local dsl_switch_valid = {
  dsl_enable = post_helper.validateBoolean
}
local dsl_switch_data = post_helper.handleQuery(dsl_switch_params,dsl_switch_valid)

local statusIcon
if is_up then
  statusIcon = "<div class=\"light green\" id = \"DSL_Status_Id\"></div>"
elseif content["dsl_status"] == "NoSignal" and content_other_line["dsl_status"] == "NoSignal" then
  statusIcon = "<div class=\"light red\" id = \"DSL_Status_Id\"></div>"
else
  statusIcon = "<div class=\"light orange\" id = \"DSL_Status_Id\"></div>"
end

local attributesAdvanced = { span = { class = "span4" } }
local attributesAjax = { span = { ["data-ajaxrefresh"] = "5",class = "span4" } }

local html = {}

if content_other_line["dsl_status"] == "Up" then
  html[#html + 1] = ui_helper.createLabel(T"DSL Status",format("%s%s",statusIcon,T(content_other_line["dsl_status"])))
else
  html[#html + 1] = ui_helper.createLabel(T"DSL Status",format("%s%s",statusIcon,T(content["dsl_status"])))
end
html[#html + 1] = ui_helper.createSwitch(T"DSL Enabled","dsl_enable",dsl_switch_data["dsl_enable"])

if is_up then
  if content["dsl_status"] == "Up" then
    attributesAjax.span.id = "dsl_uptime"
    html[#html + 1] = ui_helper.createLabel(T"DSL Uptime",post_helper.secondsToTime(content["dsl_uptime"]),attributesAjax)
    html[#html + 1] = ui_helper.createLabel(T"DSL Type",T(content["dsl_type"]),attributesAdvanced)
    html[#html + 1] = ui_helper.createLabel(T"DSL Mode",T(dsl_mode),attributesAdvanced)
  else
    attributesAjax.span.id = "dsl_uptime"
    html[#html + 1] = ui_helper.createLabel(T"DSL Uptime",post_helper.secondsToTime(content_other_line["dsl_uptime"]),attributesAjax)
    html[#html + 1] = ui_helper.createLabel(T"DSL Type",T(content_other_line["dsl_type"]),attributesAdvanced)
    html[#html + 1] = ui_helper.createLabel(T"DSL Mode",T(dsl_mode_other_line),attributesAdvanced)
  end
  attributesAjax.span.id = "dsl_max_linerate"
  html[#html + 1] =ui_helper.createLabel(T"Maximum Line Rate",dsl_max_linerate,attributesAjax)
  attributesAjax.span.id = "dsl_linerate"
  html[#html + 1] = ui_helper.createLabel(T"Line Rate",dsl_linerate,attributesAjax)
  attributesAjax.span.id="dsl_transfered"
  html[#html + 1] = ui_helper.createLabel(T"Data Transferred",dsl_transfered,attributesAjax)
  attributesAjax.span.id="dsl_power"
  html[#html + 1] = ui_helper.createLabel(T"Output Power",dsl_power,attributesAjax)
  attributesAjax.span.id="dsl_attenuation"
  html[#html + 1] = ui_helper.createLabel(T"Line Attenuation",dsl_attenuation,attributesAjax)
  attributesAjax.span.id="dsl_margin"
  html[#html + 1] = ui_helper.createLabel(T"Noise Margin",dsl_margin,attributesAjax)
end

ngx.print('\
<fieldset>\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
  <legend>',T"Physical Link Status",'</legend>\
  ',html,'\
</fieldset>\
');