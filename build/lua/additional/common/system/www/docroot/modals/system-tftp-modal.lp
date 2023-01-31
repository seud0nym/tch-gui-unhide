--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local lfs = require("lfs")
local proxy = require("datamodel")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local uinetwork = require("web.uinetwork_helper")
local find,format,match,sub,toupper = string.find,string.format,string.match,string.sub,string.upper
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local hosts_ac = uinetwork.getAutocompleteHostsList()

local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString
local vSIIPv4 = post_helper.validateStringIsIPv4

local board_names = {
  {"VANT-F",T"VANT-F : TG799vac Gateway Max"},
  {"VANT-Y",T"VANT-Y : TG800vac Gateway Max 2"},
  {"VBNT-J",T"VBNT-J : DJN2130 Frontier Gateway"},
  {"VBNT-V",T"VBNT-V : DJA0230 Smart Modem Gen 1"},
  {"VCNT-A",T"VCNT-A : DJA0231 Smart Modem Gen 2"},
}

local current_directory = untaint(proxy.get("uci.dhcp.dnsmasq.@main.tftp_root")[1].value)
if current_directory ~= "" and sub(current_directory,-1,-1) ~= "/" then
  current_directory = current_directory.."/"
end

--region usb
local usb = {}
local usbdev_data = proxy.getPN("sys.usb.device.", true)
if usbdev_data then
  for _,v in ipairs(usbdev_data) do
    local partitions = proxy.get(v.path.."partitionOfEntries")
    if partitions then
      partitions = partitions[1].value
      if partitions ~= "0" then
        local partition = proxy.getPN(v.path .. "partition.", true)
        local mount = proxy.get(partition[1].path.."path")
        if mount then
          local point = match(untaint(mount[1].value),"sd(%S%d+)")
          usb[#usb+1] = format("^/tmp/run/mountd/sd%s",point)
          usb[#usb+1] = format("^/mnt/usb/USB%%-%s",toupper(point))
        end
      end
    end
  end
end
--endregion usb

--region uploads
local function receive_file(search)
  local upload = require("web.fileupload")
  local form,errmsg = upload.fromstream()
  if not form then
    return false,1,"Failed to create upload ctx: "..errmsg
  end
  local file
  local filename
  local discard = false
  while true do
    local token,data,err = form:read()
    if not token then
      return false,2,"Read failed: "..err
    end
    if token == "header" then
      if not discard and not file and find(data[2],search,1,true) then
        filename = match(data[2],'filename="([^"]+)"')
        file,err = io.open(current_directory..filename,"w")
      end
      if not discard and not file then
        return false,3,"Failed to start receiving file: "..err
      end
    elseif token == "body" then
      if file then
        file:write(data)
      end
    elseif token == "part_end" then
      if file then
        file:close()
        file = nil
        discard = true
      end
    elseif token == "eof" then
      break
    end
  end
  return true,-1,filename
end

local function add_record(base,values,key)
  if not key then
    key = proxy.add(base)
  end
  local existing = proxy.getPN(base,true)
  for _,record in ipairs(existing) do
    if proxy.get(record.path.."_key")[1].value == key then
      for k,v in pairs(values) do
        if k ~= "_key" then
          proxy.set(record.path..k,v)
        end
      end
      proxy.apply()
      return true
    end
  end
  return false
end

local function add_boot(filename)
  local upper_filename = toupper(filename)
  for _,b in ipairs(board_names) do
    local boardname = b[1]
    if find(upper_filename,boardname,1,true) then
      local boot = proxy.getPN("uci.dhcp.boot.",true)
      local add = true
      for _,existing in ipairs(boot) do
        local networkid = proxy.get(existing.path.."networkid")[1].value
        if networkid == boardname then
          add = false
          break
        end
      end
      if add then
        add_record("uci.dhcp.boot.", { networkid = boardname, filename = filename })
        break
      end
    end
  end
  return nil
end

local function check_boot_files(directory)
  local boot = proxy.getPN("uci.dhcp.boot.",true)
  local remove = {}
  for _,existing in ipairs(boot) do
    if proxy.get(existing.path.."serveraddress")[1].value == ""  then
      local filename = untaint(proxy.get(existing.path.."filename")[1].value)
      if not lfs.attributes(filename,"mode") then
        remove[#remove+1] = existing.path
      end
    end
  end
  if #remove > 0 then
    for i=#remove,1,-1 do
      local retval,errmsg = proxy.del(remove[i])
      if not retval then
        ngx.log(ngx.ERR,format("Failed to delete %s : %s",remove[i],errmsg))
      end
    end
    proxy.apply()
    boot = proxy.getPN("uci.dhcp.boot.",true)
  end
  if directory ~= "" then
    for filename in lfs.dir(directory) do
      if filename ~= "." and filename ~= ".." then
        local add = true
        for _,existing in ipairs(boot) do
          if proxy.get(existing.path.."filename")[1].value == filename then
            add = false
            break
          end
        end
        if add then
          add_boot(filename)
          boot = proxy.getPN("uci.dhcp.boot.",true)
        end
      end
    end
  end
end

if ngx.req.get_method() == "POST" then
  local action = ngx.req.get_uri_args().action
  -- do not call get_post_args before we know that we don't have a file sent otherwise it will fail
  if action == "upload_firmware" then
    local rcv_ok,rcv_errno,rcv_result = receive_file('name="firmwarefile"')
    if not rcv_ok then
      ngx.log(ngx.ERR,rcv_result)
      ngx.print('{ "error":"'..rcv_errno..'" }')
      ngx.exit(ngx.OK)
    end
    add_boot(rcv_result)
    ngx.print('{ "success":"true" }')
    ngx.exit(ngx.HTTP_OK)
  end
end
--endregion uploads

local mapParams = {
  enabled = "uci.dhcp.dnsmasq.@main.enable_tftp",
  folder = "uci.dhcp.dnsmasq.@main.tftp_root",
}
local mapValid = {
  enabled = vB,
  folder = function(value,data)
    if data.enabled == "0" then
      return true
    end
    local path = untaint(value)
    if path == current_directory or path.."/" == current_directory then
      return true
    end
    if value == "" then
      check_boot_files(path)
    else
      for _,p in ipairs(usb) do
        if match(path,p) then
          local folder = match(path,p.."/(.+)$")
          if folder then
            local retval,errmsg = proxy.set("rpc.gui.util.nobody_dir",path)
            if not retval then
              return retval,errmsg
            end
            check_boot_files(path)
            return true
          end
          return nil,"Cannot use use the root directory of the USB device - must be a sub-directory"
        end
      end
      return nil,"Not a USB device"
    end
    return true
  end,
}
local content,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if content.enabled ~= "1" then
  content.enabled = "0"
end

--region vendor class
local vendorclass_columns = {
  {
    header = T"Vendor Class <span class='icon-question-sign' title='The value from the vendor-class tag in a DHCPREQUEST message.'></span>",
    name = "vendorclass",
    param = "vendorclass",
    type = "text",
    attr = { input = { class="span3" } },
  },
  {
    header = T"Network ID <span class='icon-question-sign' title='The Network ID To be asssociated with this Vendor Class so that the correct file is sent.'></span>",
    name = "networkid",
    param = "networkid",
    type = "text",
    attr = { input = { class="span3" } },
  },
}
local vendorclass_valid = {
  vendorclass = vNES,
  networkid = vNES,
}
local vendorclass_options = {
  canEdit = content.enabled == "1",
  canAdd = content.enabled == "1",
  canDelete = content.enabled == "1",
  tableid = "vendorclass",
  basepath = "uci.dhcp.vendorclass.@.",
  createMsg = T"Add Vendor Class",
  sorted = function(a,b)
    return toupper(a.vendorclass) < toupper(b.vendorclass)
  end,
  onAdd = function(key,values)
    add_record("uci.dhcp.vendorclass.",values,key)
  end,
}
local vendorclass_data,vendorclass_helpmsg = post_helper.handleTableQuery(vendorclass_columns,vendorclass_options,nil,nil,vendorclass_valid)
--endregion vendor class

--region boot
local boot_columns = {
  {
    header = T"Network ID <span class='icon-question-sign' title='The Network ID for most Technicolor devices is the board name (e.g. VCNT-A), except for VCNT-8 which has neither a Vendor Class or unique Tag. Turn on detailed DHCP logging to determine the correct Network ID from the Tags or Vendor Class fields.'></span>",
    name = "networkid",
    param = "networkid",
    type = "text",
    unique = true,
  },
  {
    header = T"Filename <span class='icon-question-sign' title='The name of the file to be sent via TFTP when requested through a BOOTP message.'></span>",
    name = "filename",
    param = "filename",
    type = "text",
    attr = { input = { class="span3" } },
  },
  {
    header = T"Server Address",
    name = "serveraddress",
    param = "serveraddress",
    type = "text",
    attr = { input = { class="span2",maxlength="15" },autocomplete=hosts_ac },
  },
  {
    header = T"Server Name",
    name = "servername",
    param = "servername",
    type = "text",
    attr = { input = { class="span2",maxlength="15" } },
  },
}
if current_directory ~= "" then
  local files = {}
  for filename in lfs.dir(current_directory) do
    if filename ~= "." and filename ~= ".." then
      files[#files+1] = { filename, T(filename) }
    end
  end
  table.sort(files,function(a,b)
    return a[1] < b[1]
  end)
  files[#files+1] = { ":remote;", T("(Other File on Server Address)") }
  if ngx.var.request_method == "POST" then
    local args = ngx.req.get_post_args()
    if args.filename then
      files[#files] = { args.filename, T(args.filename) }
    end
  end
  boot_columns[2]["type"] = "select"
  boot_columns[2]["values"] = files
  boot_columns[3]["attr"]["input"]["placeholder"] = "Leave blank for Gateway"
  boot_columns[4]["attr"]["input"]["placeholder"] = "Leave blank for Gateway"
end
if ngx.var.request_method == "POST" then
  local args = ngx.req.get_post_args()
  if args.action == "TABLE-NEW-LIST" then
    if args.listid == "1" then
      for _,v in ipairs(vendorclass_data) do
        board_names[#board_names+1] = { v[2], T(v[1]) }
      end
      boot_columns[1]["type"] = "select"
      boot_columns[1]["values"] = board_names
    end
  end
end
local boot_valid = {
  networkid = vNES,
  filename = function(value,data)
    local retval,errmsg = vNES(value)
    if retval then
      if data.serveraddress ~= "" or current_directory == "" then
        return retval
      end
      local path = string.format("%s/%s",current_directory,value)
      if not lfs.attributes(path,"mode") then
        return nil,path.." does not exist"
      end
      return retval
    end
    return retval,errmsg
  end,
  serveraddress = function(value)
    if current_directory ~= "" and value == "" then
      return true
    end
    return vSIIPv4(value)
  end,
  servername = function(value,data)
    if current_directory ~= "" and data.serveraddress == "" then
      return true
    end
    return vNES(value)
  end,
}
local boot_options = {
  canEdit = content.enabled == "1",
  canAdd = content.enabled == "1",
  canDelete = content.enabled == "1",
  tableid = "boot",
  basepath = "uci.dhcp.boot.@.",
  createMsg = T"Add Boot Entry",
  newList = {
    { text = "Select from known Network IDs", values = {} },
  },
  sorted = function(a,b)
    return toupper(a.networkid) < toupper(b.networkid)
  end,
  onAdd = function(key,values)
    add_record("uci.dhcp.boot.",values,key)
  end,
}
local boot_data,boot_helpmsg = post_helper.handleTableQuery(boot_columns,boot_options,nil,nil,boot_valid)
--endregion boot

--region attributes
local monitor_enable = {
  input = {
    class = "monitor-changes",
  }
}
local monitor_apply = {
  group = {
    class = "monitor-enabled monitor-1",
  },
  input = {
    class = "span7",
    placeholder = "e.g. /mnt/usb/USB-A1/firmware"
  }
}
local upload_firmware = {
  button = {
    class = "upload-firmware custom-handler",
    id = "btn-upload"
  }
}
local uploading_nofile = {
  alert = {
    class = "alert-error hide",
    id = "upload-nofile-msg"
  }
}
local uploading_alert = {
  alert = {
    class = "alert-info hide",
    id = "uploading-msg"
  }
}
local uploading_fail = {
  alert = {
    class = "alert-error hide",
    id = "upload-failed-msg"
  }
}
--endregion attributes

ngx.print(ui_helper.createHeader(T"System Extras",false,false),'\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-system-extras.lp")
ngx.print(
  ui_helper.createMessages(message_helper.popMessages()),'\
  <form class="form-horizontal" method="post" action="modals/system-tftp-modal.lp">\
    ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>',T"TFTP Server",'</legend>',
      ui_helper.createSwitch(T"Enabled","enabled",content["enabled"],monitor_enable,helpmsg["enabled"]),
      ui_helper.createInputText(T"Directory Name<span class='icon-question-sign' title='The name of the directory containing the TFTP BOOTP files, which must be a sub-directory on a mounted USB device. If it does not exist, it will be created.'></span>","folder",content["folder"],monitor_apply,helpmsg["folder"]),'\
    </fieldset>\
    <div class="control-group monitor-enabled monitor-1 monitor-hide-enable">\
      <label class="control-label">',T"Vendor Classes",'</label>\
      <div class="controls">',
        ui_helper.createTable(vendorclass_columns,vendorclass_data,vendorclass_options,nil,vendorclass_helpmsg),'\
      </div>\
    </div>\
    <div class="control-group monitor-enabled monitor-1 monitor-hide-enable">\
      <label class="control-label">',T"Boot Files",'</label>\
      <div class="controls">',
      ui_helper.createTable(boot_columns,boot_data,boot_options,nil,boot_helpmsg),'\
      </div>\
    </div>\
  </form>')
if content["folder"] ~= "" then
  ngx.print('\
  <form class="form-horizontal" id="form-upload" method="post" action="modals/system-tftp-modal.lp?action=upload_firmware" enctype="multipart/form-data">\
    <div class="control-group monitor-enabled monitor-1 monitor-hide-enable">\
      <label class="control-label">',T"Upload Firmware File<span class='icon-question-sign' title='If the name of the file contains a Technicolor board id (e.g. VCNT-A), a Boot Files record will be automatically created if one does not exist for that Network ID.'></span>",'</label>\
      <div class="controls">\
        <input type="file" name="firmwarefile" id="file-upload" class="span7">&nbsp;',ui_helper.createSimpleButton(T"Upload","icon-upload-alt",upload_firmware),'\
      </div>\
    </div>',
    ui_helper.createAlertBlock(T"No file selected.", uploading_nofile),
    ui_helper.createAlertBlock(T"Uploading firmware. Please wait...", uploading_alert),
    ui_helper.createAlertBlock(T"Uploading failed! Please try again.", uploading_fail),'\
  </form>')
end
ngx.print('\
</div>\
<script src="/js/system-tftp-modal.js"></script>',
ui_helper.createFooter());
