--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local lfs = require( "lfs" )
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local find,match = string.find,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local function comma_value(n) -- credit http://richard.warburton.it
	local left,num,right = string.match(n,'^([^%d]*%d)(%d*)(.-)$')
	return left..(num:reverse():gsub('(%d%d%d)','%1,'):reverse())..right
end

local function name_sort(a,b)
  return a[1] < b[1]
end

local space_used = 0
local status = "No"
local usbpath

local data = proxy.getPN("rpc.gui.cron.entries.",true)
for _,v in ipairs(data) do
  local cmd = proxy.get(v.path.."command")
  if cmd and find(untaint(cmd[1].value),"mtd%-backup") then
    local script = match(untaint(cmd[1].value),"(.+mtd%-backup).*")
    local cmdio = io.popen(script.." -P")
    if cmdio then
      local cmdout = cmdio:read()
      if cmdout and cmdout ~= "" then
        usbpath = cmdout
      end
      cmdio:close()
    end
    local enabled = proxy.get(v.path.."enabled")
    if enabled and enabled[1].value ~= "0" then
      status = "Yes"
    end
    break
  end
end

local backups = {}
local images = {}

if usbpath then
  local checksum = {}
  for file in lfs.dir(usbpath) do
    local basename = match(file,"(.*%.img)%.sha256")
    if basename then
      local f = io.open(usbpath.."/"..file,"r")
      if f then
        local sha256 = f:read()
        f:close()
        checksum[basename] = sha256
      end
    end
  end

  for file in lfs.dir(usbpath) do
    if file ~= "." and file ~= ".." and file ~= "README!" then
      local attributes = lfs.attributes(usbpath.."/"..file)
      local basename = match(file,"(.*%.img)%.gz")
      if basename then
        images[#images + 1] = { file,os.date("%Y/%m/%d %H:%M:%S",attributes.modification),comma_value(attributes.size),checksum[basename],}
        space_used = space_used + attributes.size
      else
        basename = match(file,"(.*%.img)%.sha256")
        if not basename then
          backups[#backups + 1] = { file,os.date("%Y/%m/%d %H:%M:%S",attributes.modification),comma_value(attributes.size),}
          space_used = space_used + attributes.size
        end  
      end
    end
  end

  table.sort(backups,name_sort)
  table.sort(images,name_sort)
end

local image_columns = {
  { --[1]
    header = T"MTD Image",
    type = "text",
  },
  { --[2]
    header = T"Date",
    type = "text",
  },
  { --[3]
    header = T"Size (Bytes)",
    type = "text",
  },
  { --[4]
    header = T"Checksum",
    type = "text",
  },
}

local image_options = {
  canEdit = false,
  canAdd = false,
  canDelete = false,
  tableid = "imagetable",
}

local backup_columns = {
  { --[1]
    header = T"Backup",
    type = "text",
  },
  { --[2]
    header = T"Date",
    type = "text",
  },
  { --[3]
    header = T"Size (Bytes)",
    type = "text",
  },
}

local backup_options = {
  canEdit = false,
  canAdd = false,
  canDelete = false,
  tableid = "backuptable",
}

  ngx.print('\
');  ngx.print(ui_helper.createHeader(T"Management",false,true) ); ngx.print('\
<div class="modal-body update">\
');  
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-management.lp")
  ngx.print('\
');  
-- dummy form so that refresh button knows which page to reload,keep it
ngx.print('\
<style>#mtdbackup table th:nth-child(3),#mtdbackup table td:nth-child(3){text-align:right;}</style>\
<form class="form-horizontal" method="post" action="modals/mtd-backup-modal.lp" id="mtdbackup">\
  <fieldset>\
    <legend>');  ngx.print( T"Status" ); ngx.print('</legend>')
    ngx.print(ui_helper.createLabel(T"Backup Enabled",status))
    if usbpath then
      local span_attr = {
        span = {
          style = "width:unset;"
        },
      }
      ngx.print(ui_helper.createLabel(T"Backup Path",usbpath,span_attr))
      ngx.print(ui_helper.createLabel(T"Space Used",comma_value(space_used).." bytes",span_attr))
    else
      local info_attr = {
        alert = {
          class = "alert-info alert"
        },
      }
      ngx.print(ui_helper.createAlertBlock(T("Backup does not appear to be configured. Run the <b>mtd-backup</b> script with the <b>-C</b> option to schedule a daily backup."),info_attr))
    end
    ngx.print('\
  </fieldset>\
  <fieldset>\
    <legend>');  ngx.print( T"MTD Images" ); ngx.print('</legend>')
    ngx.print(ui_helper.createTable(image_columns,images,image_options))
    ngx.print('\
  </fieldset>\
  <fieldset>\
    <legend>');  ngx.print( T"Configuration Backups" ); ngx.print('</legend>')
    ngx.print(ui_helper.createTable(backup_columns,backups,backup_options))
    ngx.print('\
  </fieldset>\
</form>\
</div>\
');  ngx.print(ui_helper.createFooter() ); ngx.print('\
');
