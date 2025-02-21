--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local proxy_helper = require("proxy_helper")
local ui_helper = require("web.ui_helper")

---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean

local net_intfs_path = "rpc.network.interface."
local all_intfs = content_helper.convertResultToObject(net_intfs_path.."@.",proxy.get(net_intfs_path))
local lan_intfs = {}
for _,v in ipairs(all_intfs) do
  if v.paramindex ~= "loopback" and v.up == "1" and v.type ~= "wan" and v.paramindex ~= "ppp" and v.paramindex ~= "ipoe" and v.paramindex ~= "wg0" and v.proto ~= "none" and v.proto ~= "wireguard" then
    if not proxy.getPN("uci.mcastsnooping.interface.@"..v.paramindex..".",true) then
      if v.name and v.name ~= "" then
        lan_intfs[#lan_intfs+1] = {v.paramindex, T(v.name)}
      else
        lan_intfs[#lan_intfs+1] = {v.paramindex, T(v.paramindex)}
      end
    end
  end
end

local mcsnooper_columns = {
  { --[1]
    header = T"Interface",
    name = "paramindex",
    param = "paramindex",
    type = "select",
    attr = { input = { class="span1 no-save" } },
    values = lan_intfs,
  },
  { --[2]
    header = T"IGMP<br>Snooping",
    name = "igmp_snooping",
    param = "igmp_snooping",
    type = "switch",
    attr = { switch = { ["data-placement"] = "left",class="no-save" }},
  },
  { --[3]
    header = T"MLD<br>Snooping",
    name = "mld_snooping",
    param = "mld_snooping",
    type = "switch",
    attr = { switch = { ["data-placement"] = "left",class="no-save" }},
  },
  { --[4]
    header = T"IGMP LAN-LAN<br>Snooping",
    name = "igmplan2lan_snooping",
    param = "igmplan2lan_snooping",
    type = "switch",
    attr = { switch = { ["data-placement"] = "left",class="no-save" }},
  },
  { --[5]
    header = T"MLD LAN-LAN<br>Snooping",
    name = "mldlan2lan_snooping",
    param = "mldlan2lan_snooping",
    type = "switch",
    attr = { switch = { ["data-placement"] = "left",class="no-save" }},
  },
  { --[6]
    header = T"Multicast<br>Flooding",
    name = "mcast_flooding",
    param = "mcast_flooding",
    type = "switch",
    attr = { switch = { ["data-placement"] = "left",class="no-save" }},
  },
  { --[7]
    header = T"Multicast 6<br>Flooding",
    name = "mcast6_flooding",
    param = "mcast6_flooding",
    type = "switch",
    attr = { switch = { ["data-placement"] = "left",class="no-save" }},
  },
}

local mcsnooper_valid = {
  paramindex = gVIES(lan_intfs),
  igmp_snooping = vB,
  igmplan2lan_snooping = vB,
  mcast_flooding = vB,
  mcast6_flooding = vB,
  mld_snooping = vB,
  mldlan2lan_snooping = vB,
}

local mcsnooper_options = {
  tableid = "mcsnooper",
  basepath = "uci.mcastsnooping.interface.@.",
  createMsg = T"Add Multicast Snooping Interface",
  addNamedObject = true,
  canEdit = true,
  canAdd = (#lan_intfs > 0),
  canDelete = true,
  onAdd = function(index,content)
    local path = "uci.mcastsnooping.interface.@"..index
    proxy_helper.set(path..".igmp_snooping",untaint(content.igmp_snooping))
    proxy_helper.set(path..".igmplan2lan_snooping",untaint(content.igmplan2lan_snooping))
    proxy_helper.set(path..".mcast_flooding",untaint(content.mcast_flooding))
    proxy_helper.set(path..".mcast6_flooding",untaint(content.mcast6_flooding))
    proxy_helper.set(path..".mld_snooping",untaint(content.mld_snooping))
    proxy_helper.set(path..".mldlan2lan_snooping",untaint(content.mldlan2lan_snooping))
    proxy.apply()
  end
}

local function mcsnooper_filter(content)
  if content.paramindex == "lan" then
    return { canDelete = false, canEdit = true }
  end
  return true
end

if ngx.var.request_method == "POST" then
  local postargs = ngx.req.get_post_args()
  if postargs.tableid == "mcsnooper" and postargs.action == "TABLE-EDIT" then
    mcsnooper_columns[1].readonly = true
  end
end

local mcsnooper_data,mcsnooper_helpmsg = post_helper.handleTableQuery(mcsnooper_columns,mcsnooper_options,mcsnooper_filter,nil,mcsnooper_valid)

ngx.print(ui_helper.createHeader(T"Local Network",false,false),'\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-lan.lp")
ngx.print(
  ui_helper.createMessages(message_helper.popMessages()),'\
  <form class="form-horizontal" method="post" action="modals/ethernet-mcsnooper-modal.lp">\
    <fieldset>\
      <legend>',T"Multicast Snooping",'</legend>',
        ui_helper.createTable(mcsnooper_columns,mcsnooper_data,mcsnooper_options,nil,mcsnooper_helpmsg),'\
    </fieldset>\
  </form>\
  <script src="/js/modal-busy.js"></script>\
</div>\
',ui_helper.createFooter())
