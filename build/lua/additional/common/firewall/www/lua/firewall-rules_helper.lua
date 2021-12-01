--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
-- Process POST query

local M = {}

local ngx = ngx
local post_helper = require("web.post_helper")
local content_helper = require("web.content_helper")
local portslist = require("portslist_helper")
local hosts_ac,hosts_ac_v6 = require("web.uinetwork_helper").getAutocompleteHostsList()
local concat,remove = table.concat,table.remove
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local find,match = string.find,string.match

local vB = post_helper.validateBoolean
local gVIES = post_helper.getValidateInEnumSelect
local vSIPR = post_helper.validateStringIsPortRange
local gOV = post_helper.getOptionalValidation
local vIP4AS = post_helper.validateIPAndSubnet(4)
local vIP6AS = post_helper.validateIPAndSubnet(6)
local gDSM = post_helper.getDefaultSubnetMask
local netMaskToMask = post_helper.cidr2mask
local isNA = post_helper.isNetworkAddress


local rpc_fw_zone_path = "uci.firewall.zone."
local rpc_fw_zone_content = content_helper.getMatchedContent(rpc_fw_zone_path)
local dst_intfs = {
  {"",T"<i>Incoming Rule</i>"},
}
local src_intfs = {
  {"",T"<i>Outgoing Rule</i>"},
}
for _,v in ipairs (rpc_fw_zone_content) do
  dst_intfs[#dst_intfs+1] = { v.name,T(v.name) }
  src_intfs[#src_intfs+1] = { v.name,T(v.name) }
end
dst_intfs[#dst_intfs+1] = { "*",T"ALL" }
src_intfs[#src_intfs+1] = { "*",T"ALL" }

local fwrules_family = {
  { "",T""},
  { "ipv4",T"IPv4"},
  { "ipv6",T"IPv6"},
}
-- WARNING! Probably incomplete: derived from existing configured values
local fwrules_icmp_types = {
  { "echo-reply",T"echo-reply" },
  { "destination-unreachable",T"destination-unreachable" },
  { "echo-request",T"echo-request" },
  { "router-advertisement",T"router-advertisement" },
  { "router-solicitation",T"router-solicitation" },
  { "neighbour-advertisement",T"neighbour-advertisement" },
  { "neighbour-solicitation",T"neighbour-solicitation" },
  { "time-exceeded",T"time-exceeded" },
  { "packet-too-big",T"packet-too-big" },
  { "bad-header",T"bad-header" },
  { "unknown-header-type",T"unknown-header-type" },
}

local fwrules_targets = {
  { "ACCEPT",T"ACCEPT"},
  { "DROP",T"DROP"},
  { "REJECT",T"REJECT"},
}

local fwrules_protocols = {
  { "",T"TCP"},
  { "tcp",T"TCP"},
  { "udp",T"UDP"},
  { "tcpudp",T"TCP/UDP"},
  { "icmp",T"ICMP"},
  { "esp",T"ESP"},
  { "ah",T"AH"},
  { "sctp",T"SCTP"},
  { "all",T"all"},
}

local fwrules_v6_protocols = {
  { "",T"TCP"},
  { "tcp",T"TCP"},
  { "udp",T"UDP"},
  { "tcpudp",T"TCP/UDP"},
  { "icmpv6",T"ICMPv6"},
  { "esp",T"ESP"},
  { "ah",T"AH"},
  { "sctp",T"SCTP"},
  { "all",T"all"},
}

local duplicatedErrMsg = nil
local session = ngx.ctx.session
--[[
   The following function used to validate the duplicate entries while adding or editing on firewall table.
   We will throw the duplicate error if any rows containing all 6 values below are duplicated
   Sample:- Protocol    Src IP       Src port      Dst IP         Dst port    DSCP
            TCP         192.168.1.1  1000          192.168.1.4    2000        AF23
            TCP         192.168.1.1  1000          192.168.1.5    2000        CS7
            UDP         192.168.1.1  1000          192.168.1.5    2001        AF23
            TCP         192.168.1.1  1000          192.168.1.5    2000        EF
   In above example the 4th and 2nd rows are exactly duplicated. In this case we throw the error message as duplicated.
   If anyone of the value of row is different then we don't consider as duplicated row.
]]
local function rulesDuplicateCheck(basepath,tableid,columns)
  return function(value,postdata,key)
  local success,msg
    if value and value ~= "" then
      success,msg = vSIPR(value,postdata,key)
    else
      success = true
    end
    if success then
      -- specify column range to check for duplicates
      local startIndex,endIndex = 3,13
      local fullpath = nil
      if postdata.action =="TABLE-ADD" or postdata.action =="TABLE-MODIFY" then
        local index = tonumber(postdata.index)
        local tablesessionindexes = tableid..".allowedindexes"
        local allowedIndexes = session:retrieve(tablesessionindexes) or {}
        if allowedIndexes[index] then
          index = allowedIndexes[index].paramindex
        end
        -- fullpath => The UCI path which is going to be modifed,Ex: rpc.network.firewall.userrule.@4.
        fullpath = basepath.."@"..index.."."
      end
      local paths=nil
      for i=startIndex,endIndex do
        local value = untaint(postdata[columns[i].name])
        local cmatch = content_helper.getMatchedContent(basepath,{[columns[i].param] = value })
        if fullpath then
          for u,v in ipairs(cmatch) do
            if v.path == fullpath then
            --The rpc.network.firewall.userrule.@4. will be removed
            --because we no need to validate with the path which we need to modify
              remove(cmatch,u)
              break
            end
          end
        end
        -- If cmatch is empty then there will be no duplicated rows in UCI.
        if #cmatch > 0 then
        -- The below condition will be true at first iteration.
          if i == startIndex then
            -- At the first iteration the duplicate paths will be stored in a temp table
            -- which can be used to validate with duplicate path of subsequest columns
            paths={}
            for _,v in ipairs(cmatch) do
              paths[v.path]=true
            end
          -- If path is empty then no duplicates in previous columns. So we can break the loop and can say no duplicates
          elseif paths then
            local duplicate = {}
            local flag = false
            for _,v in ipairs(cmatch) do
              if paths[v.path] then
                duplicate[v.path] = true
                flag = true
              end
            end
            -- if current duplicated path is not matching with previouse duplicatd paths.
            -- Then there is no exact duplicated rows.
            if flag then
              paths = duplicate
            else
               paths = nil
               break
            end
          else
            paths = nil
            break
          end
        else
          paths=nil
          break
        end
      end
      --Finally if you get one or more paths which contain all
      --the 4 values are duplicated (sr ip,port and dest ip,port) are duplicated
      if paths then
          success = nil
          msg = T"Existing rule duplicates these values"
          duplicatedErrMsg = msg
      end
    end
    return success,msg
  end
end

local function validateLanIP(value,object,key)
  local retVal,msg
  local ipAddress,netMask = match(value,"^([^/]+)/?(%d*)$")
  retVal,msg = gOV(vIP4AS(value))
  if retVal and netMask == "" and ((key == "src_ip" and object.src == "lan") or (key == "dest_ip" and object.dest == "lan")) then
    --To add default subnet mask to the IPv4 Network Address if not explicitly mentioned.
    netMask = gDSM(ipAddress)
    if netMask then
      local isNetworkAddress = isNA(ipAddress,netMaskToMask(netMask))
      if isNetworkAddress then
        object[key] = value.."/"..netMask
      end
    end
    return true
  end
  return retVal,msg
end

function M.getRuleColumns(fwrule_options)
  local dup_chk_basepath = match(fwrule_options.basepath,"^(.+)@%.$")
  local isIPv6 = false
  if find(fwrule_options.basepath,"_v6") then
    isIPv6 = true
  end

  duplicatedErrMsg = nil

  local fwrule_columns = {
    { -- [1]
      header = "",
      name = "status",
      param = "enabled",
      type = "switch",
      readonly = true,
      default = "1",
      attr = { switch = { ["data-placement"] = "right" }},
    },
    { -- [2]
      header = T"Name",
      name = "name",
      param = "name",
      type = "text",
      readonly = true,
    },
    { -- [3]
      header = T"Family",
      name = "family",
      param = "family",
      type = "select",
      values = fwrules_family,
      readonly = true,
    },
    { -- [4]
      header = T"Action",
      name = "target",
      param = "target",
      type = "text",
      readonly = true,
    },
    { -- [5]
      header = T"Protocol",
      name = "protocol",
      param = "proto",
      type = "select",
      values = fwrules_protocols,
      readonly = true,
    },
    { -- [6]
      header = T"ICMP<br>Type",
      name = "icmp_type",
      param = "icmp_type",
      type = "checkboxgroup",
      values = fwrules_icmp_types,
      readonly = true,
    },
    { -- [7]
      header = T"Src<br>Zone",
      name = "src",
      param = "src",
      type = "text",
      readonly = true,
    },
    { -- [8]
      header = T"Src<br>MAC",
      name = "src_mac",
      param = "src_mac",
      type = "text",
      readonly = true,
      },
    { -- [9]
      header = T"Src IP/<br>Subnet",
      name = "src_ip",
      param = "src_ip",
      type = "text",
      readonly = true,
    },
    { -- [10]
      header = T"Src<br>Port",
      name = "src_port",
      param = "src_port",
      type = "text",
      readonly = true,
    },
    { -- [11]
      header = T"Dest<br>Zone",
      name = "dest",
      param = "dest",
      type = "text",
      readonly = true,
    },
    { -- [12]
      header = T"Dest<br>MAC",
      name = "dest_mac",
      param = "dest_mac",
      type = "text",
      readonly = true,
    },
    { -- [13]
      header = T"Dest IP/<br>Subnet",
      name = "dest_ip",
      param = "dest_ip",
      type = "text",
      readonly = true,
    },
    { -- [14]
      header = T"Dest<br>Port",
      name = "dest_port",
      param = "dest_port",
      type = "text",
      readonly = true,
    },
    { -- [15]
      header = T"IP<br>Set",
      name = "ipset",
      param = "ipset",
      type = "text",
      readonly = true,
    },
    { -- [16]
      header = "",
      legend = T"Firewall Rule",
      name = "fwrule_entry",
      type = "aggregate",
      synthesis = nil,--tod_aggregate,
      subcolumns = {
        { -- [1]
          header = "Enabled",
          name = "enabled",
          param = "enabled",
          type = "switch",
          default = "1",
          attr = { switch = { ["data-placement"] = "right" }}
        },
        { -- [2]
          header = T"Name",
          name = "name",
          param = "name",
          type = "text",
          attr = { input = { class="span2" } },
        },
        { -- [3]
          header = T"Action",
          name = "target",
          param = "target",
          default = "DROP",
          type = "select",
          values = fwrules_targets,
          attr = { select = { class="span2" } },
        },
        { -- [4]
          header = T"Protocol",
          name = "protocol",
          param = "proto",
          default = "tcp",
          type = "select",
          values = fwrules_protocols,
          attr = { select = { class="span2" } },
        },
        { -- [5]
          header = T"Src Zone",
          name = "src",
          param = "src",
          default = "wan",
          type = "select",
          values = src_intfs,
          attr = { select = { class="span2" } },
        },
        { -- [6]
          header = T"Src MAC",
          name = "src_mac",
          param = "src_mac",
          type = "text",
          attr = { input = { class="span2" } },
        },
        { -- [7]
          header = T"Src IP/Subnet",
          name = "src_ip",
          param = "src_ip",
          type = "text",
          attr = { input = { class="span2",maxlength="18" },autocomplete = hosts_ac },
        },
        { -- [8]
          header = T"Src Port",
          name = "src_port",
          param = "src_port",
          type = "text",
          attr = { input = { class="span1",maxlength="11" },autocomplete = portslist },
        },
        { -- [9]
          header = T"Dest Zone",
          name = "dest",
          param = "dest",
          default = "",
          type = "select",
          values = dst_intfs,
          attr = { select = { class="span2" } },
        },
        { -- [10]
          header = T"Dest MAC",
          name = "dest_mac",
          param = "dest_mac",
          type = "text",
          attr = { input = { class="span2",maxlength="18" } },
        },
        { -- [11]
          header = T"Dest IP/Subnet",
          name = "dest_ip",
          param = "dest_ip",
          type = "text",
          attr = { input = { class="span2",maxlength="18" },autocomplete = hosts_ac },
        },
        { -- [12]
          header = T"Dest Port",
          name = "dest_port",
          param = "dest_port",
          type = "text",
          attr = { input = { class="span1",maxlength="11" },autocomplete = portslist },
        },
        { -- [13]
          header = T"Family",
          name = "family",
          param = "family",
          type = "hidden",
          default = "ipv4",
          readonly = true,
        },
      },
    }
  }

  local fwrule_valid

  if isIPv6 then
    for _,v in ipairs(fwrule_columns) do
      v.name = v.name.."_v6"
      if v.param == "proto" then
        v.values = fwrules_v6_protocols
      end
      if v.name == "fwrule_entry_v6" then
        for _,w in ipairs(v.subcolumns) do
          w.name = w.name.."_v6"
          if w.param == "proto" then
            w.values = fwrules_v6_protocols
          elseif w.param == "family" then
            w.default = "ipv6"
          elseif w.param == "src_ip" or w.param == "dest_ip" then
            w.attr = { input = { class="span2",maxlength="39" },autocomplete = hosts_ac_v6 }
          end
        end
      end
    end
    fwrule_valid = {
      enabled_v6 = vB,
      target_v6 = gVIES(fwrules_targets),
      family_v6 = gVIES(fwrules_family),
      protocol_v6 = gVIES(fwrules_v6_protocols),
      src_v6 = gVIES(src_intfs),
      src_ip_v6 = gOV(vIP6AS),
      src_port_v6 = gOV(vSIPR),
      dest_v6 = gVIES(dst_intfs),
      dest_ip_v6 = gOV(vIP6AS),
      dest_port_v6 = rulesDuplicateCheck(dup_chk_basepath,fwrule_options.tableid,fwrule_columns[16].subcolumns),
    }
  else
    fwrule_valid = {
      enabled = vB,
      target = gVIES(fwrules_targets),
      family = gVIES(fwrules_family),
      protocol = gVIES(fwrules_protocols),
      src = gVIES(src_intfs),
      src_ip = validateLanIP,
      src_port = gOV(vSIPR),
      dest = gVIES(dst_intfs),
      dest_ip = validateLanIP,
      dest_port = rulesDuplicateCheck(dup_chk_basepath,fwrule_options.tableid,fwrule_columns[16].subcolumns),
    }
  end

  if fwrule_options.canEdit and not fwrule_options.canAdd and not fwrule_options.canDelete then
    fwrule_columns[1].readonly = false
    remove(fwrule_columns,16)
  end

  return fwrule_columns,fwrule_valid
end

function M.fwrule_sort(rule1,rule2)
  return tonumber(rule1.paramindex) < tonumber(rule2.paramindex)
end

function M.handleTableQuery(fwrule_options,fwrule_defaultObject)
  local fwrule_columns,fwrule_valid = M.getRuleColumns(fwrule_options)
  local fwrule_data,fwrule_helpmsg = post_helper.handleTableQuery(fwrule_columns,fwrule_options,nil,fwrule_defaultObject,fwrule_valid)
  for _,v in ipairs(fwrule_data) do
    if v[1] == "" then
      v[1] = "1"
    end
    if v[6] and type(v[6]) == "table" then
      v[6] = concat(v[6],",")
      fwrule_columns[6].type = "text"
      fwrule_columns[6].values = nil
    end
  end

  if duplicatedErrMsg then
    for _,v in ipairs(fwrule_columns[16].subcolumns) do
      if v.param ~= "enabled" and v.param ~= "name" and (not fwrule_helpmsg[v.name] or fwrule_helpmsg[v.name] == "") then
        fwrule_helpmsg[v.name]= duplicatedErrMsg
      end
    end
    duplicatedErrMsg = nil
  end

  return fwrule_columns,fwrule_data,fwrule_helpmsg
end

return M
