--pretranslated: do not change this file

-- Enable localization
gettext.textdomain("webui-core")

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local modes_content = content_helper.getMatchedContent("uci.xdsl.xdsl.@dsl0.multimode.")
local all_modes = {"adsl2","adsl2annexl","adsl2annexm","adsl2plus","gdmt","glite","t1413","vdsl2"}
table.sort(
  all_modes,
  function(a,b)
    return a < b
  end
)

if ngx.var.request_method == "POST" then
  local vars = ngx.req.get_post_args()
  if vars["action"] == "SAVE" then
    local mode2prog = {}
    for _,v in pairs(all_modes) do
      mode2prog[v] = {prog = 0,found = 0}
    end
    if type(vars["modes"]) == "table" then
      for _,x in pairs(vars["modes"]) do
        for k,_ in pairs(mode2prog) do
          if k == x then
            mode2prog[format("%s",k)].prog = 1
          end
        end
      end
    else
      for k,_ in pairs(mode2prog) do
        if k == vars["modes"] then
          mode2prog[format("%s",k)].prog = 1
        end
      end
    end
    for _,v in pairs(modes_content) do
      local found = 0
      for k,_ in pairs(mode2prog) do
        if v.value == k then
          mode2prog[format("%s",k)].found = v.path
          found = 1
        end
      end
      if found == 0 then
        mode2prog[format("%s",v.path)].found = v.path
        mode2prog[format("%s",v.path)].prog = 0
      end
    end
    for k,v in pairs(mode2prog) do
      if v.prog == 1 and v.found == 0 then
        local addvalue = proxy.add("uci.xdsl.xdsl.@dsl0.multimode.")
        proxy.set("uci.xdsl.xdsl.@dsl0.multimode.@" .. addvalue .. ".value",k)
      elseif v.prog == 0 and v.found then
        proxy.del(v.found)
      end
    end
  end
end

local content = {
  dsl0_status = "sys.class.xdsl.@line0.Status",
  dsl0_linkencapsulation = "sys.class.xdsl.@line0.LinkEncapsulationUsed",
  dsl0_mode = "rpc.xdslctl.DslMode",
  dsl0_profile = "rpc.xdslctl.DslProfile",
  dsl0_port = "rpc.xdslctl.DslamPort",
  dsl0_enabled = "uci.xdsl.xdsl.@dsl0.enabled",
  dsl0_eoc_vendor_id = "uci.xdsl.xdsl.@dsl0.eoc_vendor_id",
  dsl0_eoc_serial_number = "uci.xdsl.xdsl.@dsl0.eoc_serial_number",
  dsl0_handshake_switch_timeout = "uci.xdsl.xdsl.@dsl0.handshake_switch_timeout",
  dsl0_demod_cap_value = "uci.xdsl.xdsl.@dsl0.demod_cap_value",
  dsl0_demod_cap_mask = "uci.xdsl.xdsl.@dsl0.demod_cap_mask",
  dsl0_demod_cap2_value = "uci.xdsl.xdsl.@dsl0.demod_cap2_value",
  dsl0_demod_cap2_mask = "uci.xdsl.xdsl.@dsl0.demod_cap2_mask",
  dsl0_aux_features_value = "uci.xdsl.xdsl.@dsl0.aux_features_value",
  dsl0_aux_features_mask = "uci.xdsl.xdsl.@dsl0.aux_features_mask",
  dsl0_vdsl_cfg_flags_value = "uci.xdsl.xdsl.@dsl0.vdsl_cfg_flags_value",
  dsl0_vdsl_cfg_flags_mask = "uci.xdsl.xdsl.@dsl0.vdsl_cfg_flags_mask",
  dsl0_xdsl_cfg1_value = "uci.xdsl.xdsl.@dsl0.xdsl_cfg1_value",
  dsl0_xdsl_cfg1_mask = "uci.xdsl.xdsl.@dsl0.xdsl_cfg1_mask",
  dsl0_xdsl_FirmwareVersion = "sys.class.xdsl.@line0.FirmwareVersion",
  marketing_version = "uci.version.version.@version[0].marketing_version",
  dslam_chipset = "rpc.xdslctl.DslamChipset",
  dslam_version = "rpc.xdslctl.DslamVersion",
  dslam_version_raw = "rpc.xdslctl.DslamVersionRaw",
  dslam_serial = "rpc.xdslctl.DslamSerial"
}
content_helper.getExactContent(content)

if content.dslam_chipset:match("BDCM") then
  content.dslam_chipset = "Broadcom" .. " ( " .. content.dslam_chipset .. " )"
elseif content.dslam_chipset:match("IFTN") then
  content.dslam_chipset = "Infineon" .. " ( " .. content.dslam_chipset .. " )"
end

table.sort(modes_content,
  function(a,b)
    return match(a.path,"@(%d)") < match(b.path,"@(%d)")
  end
)

local mode_list = nil
for _,v in pairs(modes_content) do
  if not mode_list then
    mode_list = format("%s",v.value)
  else
    mode_list = format("%s,%s",mode_list,v.value)
  end
end

local profiles_content = content_helper.getMatchedContent("uci.xdsl.xdsl.@dsl0.profile.")
table.sort(
  profiles_content,
  function(a,b)
    return match(a.path,"@(%d)") < match(b.path,"@(%d)")
  end
)

local profile_list = nil
for _,v in pairs(profiles_content) do
  if not profile_list then
    profile_list = format("%s",v.value)
  else
    profile_list = format("%s,%s",profile_list,v.value)
  end
end

local driver_Params = {
  dsl0_enabled = "uci.xdsl.xdsl.@dsl0.enabled",
  ds10_sra = "uci.xdsl.xdsl.@dsl0.sra",
  ds10_bitswap = "uci.xdsl.xdsl.@dsl0.bitswap",
}
local driver_valid = {
  dsl0_enabled = post_helper.validateBoolean,
  ds10_sra = post_helper.validateBoolean,
  ds10_bitswap = post_helper.validateBoolean,
}
local driver_content,help_msg = post_helper.handleQuery(driver_Params,driver_valid)


local state_map = {
  ["0"] = T"Disabled",
  ["1"] = T"Enabled",
}

local light_map = {
  ["0"] = "off",
  ["1"] = "green",
}
local span5 = {
  span = {class = "span5"}
}

ngx.print(
  ui_helper.createHeader(T"xDSL Details",false,false),'\
  <div class="modal-body update">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <form class="form-horizontal" method="post" action="modals/xdsl-low-modal.lp">',
        ui_helper.createLight(T"Status",nil,state_map[untaint(content.dsl0_enabled)],{light = {class = light_map[untaint(content.dsl0_enabled)]}}),
        ui_helper.createSwitch(T"Enabled","dsl0_enabled",driver_content["dsl0_enabled"],help_msg["dsl0_enabled"]),
        ui_helper.createLabel(T"DSL Connection Status",T(content.dsl0_status),span5))
        if (content.dsl0_status == "Up") then
          ngx.print(ui_helper.createLabel(T"DSL Mode",content.dsl0_mode .. " (" .. content.dsl0_linkencapsulation .. ")",span5))
          if (content.dsl0_profile ~= "Not Found") then
            ngx.print(ui_helper.createLabel(T"VDSL Profile",content.dsl0_profile,span5))
          end
          if (content.dsl0_port ~= "") then
            ngx.print(ui_helper.createLabel(T"VDSL DSLAM Port",content.dsl0_port,span5))
          end
          if (content.dslam_serial ~= "") then
            ngx.print(ui_helper.createLabel(T"VDSL DSLAM Serial",content.dslam_serial,span5))
          end
          ngx.print(ui_helper.createLabel(T"DSLAM Chipset",content.dslam_chipset,span5))
          if content.dslam_version_raw:sub(0,2) == "0x" then
            ngx.print(ui_helper.createLabel(T"DSLAM Firmware",format("%s ( %s )",content.dslam_version,content.dslam_version_raw),span5))
          elseif content.dslam_version_raw == "" then
            ngx.print(ui_helper.createLabel(T"DSLAM Firmware",T"Can't recover DSLAM version.",span5))
          else
            ngx.print(ui_helper.createLabel(T"DSLAM Firmware",format(T"Invalid version,can't convert. Raw value: %s",content.dslam_version_raw),span5))
          end
        end
        ngx.print(
          ui_helper.createLabel(T"Datapump Version",content.dsl0_xdsl_FirmwareVersion,span5),
          ui_helper.createSwitch(T"SRA","ds10_sra",driver_content["ds10_sra"],help_msg["ds10_sra"]),
          ui_helper.createSwitch(T"Bitswap","ds10_bitswap",driver_content["ds10_bitswap"],help_msg["ds10_bitswap"]),
          ui_helper.createLabel(T"EOC Vendor ID",content.dsl0_eoc_vendor_id,span5),
          ui_helper.createLabel(T"EOC Serial Number",content.dsl0_eoc_serial_number,span5),
          [[<div class="control-group"><label class="control-label">]],T"Modes",[[</label>]],
            [[<div class="controls"><select multiple class="form-control" name="modes">]])
              for i = 1,#all_modes do
                local selected = ""
                for _,w in pairs(modes_content) do
                  if all_modes[i] == w.value then
                    selected = "selected"
                  end
                end
                ngx.print(format('<option name="%s" %s>%s</option>',all_modes[i],selected,all_modes[i]))
              end
              ngx.print([[</select>]],
            [[</div>]],
          [[</div>]])
          ngx.print(
            ui_helper.createLabel(T"Profiles",profile_list or "",span5),
            ui_helper.createLabel(T"HS Switch Timeout",content.dsl0_handshake_switch_timeout,span5),
            ui_helper.createLabel(T"Demod Cap Value",content.dsl0_demod_cap_value,span5),
            ui_helper.createLabel(T"Demod Cap Mask",content.dsl0_demod_cap_mask,span5),
            ui_helper.createLabel(T"2nd Demod Cap Value",content.dsl0_demod_cap2_value,span5),
            ui_helper.createLabel(T"2nd Demod Cap Mask",content.dsl0_demod_cap2_mask,span5),
            ui_helper.createLabel(T"Aux Features Value",content.dsl0_aux_features_value,span5),
            ui_helper.createLabel(T"Aux Features Mask",content.dsl0_aux_features_mask,span5),
            ui_helper.createLabel(T"VDSL Flags Value",content.dsl0_vdsl_cfg_flags_value,span5),
            ui_helper.createLabel(T"VDSL Flags Mask",content.dsl0_vdsl_cfg_flags_mask,span5),
            ui_helper.createLabel(T"Phy Extra Value",content.dsl0_xdsl_cfg1_value,span5),
            ui_helper.createLabel(T"Phy Extra Mask",content.dsl0_xdsl_cfg1_mask,span5),'\
      </form>\
    </fieldset>\
  </div>',
ui_helper.createFooter())