--pretranslated: do not change this file

gettext.textdomain('webui-core')

local lp = require("web.lp")
local post_helper = require("web.post_helper")
local message_helper = require("web.uimessage_helper")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")
local format, match = string.format, string.match

local ethname = proxy.get("sys.eth.port.@eth4.status")
if ethname and ethname[1].value then
  ethname =  "eth4"
else
  ethname =  "eth3"
end

local function set(path, value)
  local retval, errmsg = proxy.set(path, value)
  if not retval then
    local v,y
    for _,v in pairs(errmsg) do
      for _,y in pairs(v) do
        message_helper.pushMessage("Error setting "..path..": "..y, "error")
      end
    end
  end
end

local function getValidateNumberInRange(min, max, value)
  local helptext = T"Input must be a number"
  if min and max then
    helptext = format(T"Input must be a number between %d and %d included", min, max)
  elseif not min and not max then
    helptext = T"Input must be a number"
  elseif not min then
    helptext = format(T"Input must be a number smaller than %d included", max)
  elseif not max then
    helptext = format(T"Input must be a number greater than %d included", min)
  end
  local num = tonumber(value)
  if not num then
      return nil, helptext
  end
  if min and num < min then
      return nil, helptext
  end
  if max and num > max then
      return nil, helptext
  end
  return true
end

local wanifnamepath = "uci.network.interface.@wan.ifname"
local wan6ifnamepath = "uci.network.interface.@wan6.ifname"
local iface = proxy.get(wanifnamepath)
local connection = ""
local path = ""
local vid = ""
local dname = ""
local difname = ""

if iface and iface[1].value then
  iface = iface[1].value
else
  iface = ""
end

if match(iface, "eth") then
  connection = ethname
  path = "uci.network.device.@wan"..ethname.."."
  if match(iface, "%.") then
    difname = match(iface, ethname .. "%.[%d]+")
    vid = match(difname, "[^"..ethname.."%.]+")
  else
    if proxy.get(path) then
      vid = proxy.get(path.."vid")[1].value
      dname = proxy.get(path.."name")[1].value
      difname = proxy.get(path.."ifname")[1].value
    end
  end
elseif match(iface, "atm") then
  connection = "atmwan"
  path = "uci.network.device.@wanatmwan."
  if match(iface, "%.") then
    difname = match(iface, "atmwan%.[%d]+")
    vid = match(difname, "[^atmwan%.]+")
  else
    if proxy.get(path) then
      vid = proxy.get(path.."vid")[1].value
      dname = proxy.get(path.."name")[1].value
      difname = proxy.get(path.."ifname")[1].value
    end
  end
else
  connection = "ptm0"
  path = "uci.network.device.@wanptm0."
  if match(iface, "%.") then
    difname = match(iface, "ptm0%.[%d]+")
    vid = match(difname, "[^ptm0%.]+")
  else
    if proxy.get(path) then
      vid = proxy.get(path.."vid")[1].value
      dname = proxy.get(path.."name")[1].value
      difname = proxy.get(path.."ifname")[1].value
    end
  end
end

if ngx.var.request_method == "POST" then
  local content = ngx.req.get_post_args()
  if content["action"]=="SAVE" then
    range, helpmes = getValidateNumberInRange(2, 2048, content["intf_vlanid"])
    if content["auswahlliste"] == "devnot" then
      if range and (content["intf_vlanid"]~=vid or content["intf_vlan"]~="0") then
        if proxy.get(path) then
          dname = proxy.get(path.."name")[1].value
          set(path.."vid", content["intf_vlanid"])
          set(wanifnamepath, dname)
          set(wan6ifnamepath, dname)
          set(path.."ifname", connection)
        else
          proxy.add("uci.network.device.", format("%s%s","wan",connection ))
          set(path.."type","8021q")
          set(path.."name","wan"..connection)
          set(path.."macaddr", proxy.get("uci.env.rip.eth_mac")[1].value)
          dname = proxy.get(path.."name")[1].value
          set(path.."vid", content["intf_vlanid"])
          set(wanifnamepath, dname)
          set(wan6ifnamepath, dname)
          set(path.."ifname", connection)
        end
        proxy.apply()
      end
    elseif content["auswahlliste"] == "dotnot" then
      if range and (content["intf_vlanid"]~=vid or content["intf_vlan"]~="0") then
        if proxy.get(path) and proxy.get(path.."ifname")[1].value ~="" then
          set(path.."ifname", "")
        end
        local wanifname = format("%s.%s",connection, content["intf_vlanid"])
        set(wanifnamepath, wanifname)
        set(wan6ifnamepath, wanifname)
        proxy.apply()
      end
    end
    if content["intf_vlan"]=="0" then
      if proxy.get(path) then
        set(path.."ifname", "")
      end
      set(wanifnamepath, connection)
      set(wan6ifnamepath, connection)
      proxy.apply()
    end
    if match(iface, "eth") then
      connection = ethname
      path = "uci.network.device.@wan"..ethname.."."
      if match(iface, "%.") then
        difname = match(iface, ethname.."%.[%d]+")
        vid = match(difname, "[^"..ethname.."%.]+")
      else
        if proxy.get(path) then
          vid = proxy.get(path.."vid")[1].value
          dname = proxy.get(path.."name")[1].value
          difname = proxy.get(path.."ifname")[1].value
        end
      end
    elseif match(iface, "ptm") then
      connection = "ptm0"
      path = "uci.network.device.@wanptm0."
      if match(iface, "%.") then
        difname = match(iface, "ptm0%.[%d]+")
        vid = match(difname, "[^ptm0%.]+")
      else
        if proxy.get(path) then
          vid = proxy.get(path.."vid")[1].value
          dname = proxy.get(path.."name")[1].value
          difname = proxy.get(path.."ifname")[1].value
        end
      end
    else
      connection = "atmwan"
      path = "uci.network.device.@wanatmwan."
      if match(iface, "%.") then
        difname = match(iface, "atmwan%.[%d]+")
        vid = match(difname, "[^atmwan%.]+")
      else
        if proxy.get(path) then
          vid = proxy.get(path.."vid")[1].value
          dname = proxy.get(path.."name")[1].value
          difname = proxy.get(path.."ifname")[1].value
        end
      end
    end
  end
end

local check
if not iface or match(iface, "%.") then
  check = "dotnot"
else
  check = "devnot"
end

local b
if check == "dotnot" then
  if match(difname, "%.") then
    b = match(difname, "[%w]+")
    difname = match(difname, "[%w]+")
  else
    b = "empty"
  end
else
  if match(difname,"[%w]+") then
    b = match(difname,"[%w]+")
  else
    b = "empty"
  end
end

ngx.print('\
<fieldset>\
  <legend>'); ngx.print( T"Internet Channel" ); ngx.print('</legend>');

  local liste = {
    {"devnot", T"Device &nbsp;"},
    {"dotnot", T"Dot &nbsp;"}
  }

  local html = {}
  local vlanswitchclass = {
    input = {
      class = "monitor-changes"
    },
    values = {on = b, off = ""},
  }
  local vlanidclass = {
    group = {
      class = format("monitor-intf_vlan monitor-%s",b),
    },
  }
  local alertclass = {
    alert = {
      class = "alert-warn monitor-wsens monitor-1"
    },
  }
  html[#html + 1] = ui_helper.createAlertBlock(T"WAN Sensing may not inter-operate correctly with VLAN. Disable WAN Sensing and select Connection Mode first, before setting VLAN ID.", alertclass)
  html[#html + 1] = ui_helper.createSwitch(T"Using VLAN", "intf_vlan", difname, vlanswitchclass)
  html[#html + 1] = ui_helper.createInputRadio(T"Notation", "auswahlliste" , liste, check, vlanidclass)
  html[#html + 1] = ui_helper.createInputText(T"VLAN ID", "intf_vlanid", vid, vlanidclass, helpmes)
  ngx.print(html)

  ngx.print('\
</fieldset>\
');
