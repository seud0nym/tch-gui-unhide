--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")

local function sendcmd(cmd,name)
  ngx.log(ngx.INFO, "Calling "..name.." "..cmd)
	proxy.set("rpc.gui.init."..cmd, name)
  proxy.apply()
end

if ngx.req.get_method() == "POST" then
	local action = string.untaint(ngx.req.get_post_args().action)
	if action:match("restart") then
		local name = action:gsub("restart_","")
		sendcmd("restart",name)
	elseif action:match("start") then
		local name = action:gsub("start_","")
		sendcmd("start",name)
	elseif action:match("stop") then
		local name = action:gsub("stop_","")
		sendcmd("stop",name)
	elseif action:match("enable") then
		local name = action:gsub("enable_","")
		sendcmd("enable",name)
	elseif action:match("disable") then
		local name = action:gsub("disable_","")
		sendcmd("disable",name)
	end
end

local init_columns = {
	{ --[1]
		header = T"Enabled",
		name = "active",
		param = "active",
		type = "light",
	},
	{ --[2]
		header = T"Name",
		name = "name",
		param = "name",
		type = "text",
	},
	{ --[3]
		header = T"Start Order",
		name = "start_order",
		param = "start_order",
		type = "text",
	},
	{ --[4]
		header = T"Stop Order",
		name = "stop_order",
		param = "stop_order",
		type = "text",
	},
	{ --[5]
		header = T"Toggle",
		name = "toggle",
		param = "toggle",
		type = "text",
	},
	{ --[6]
		header = T"Start",
		name = "start",
		param = "start",
		type = "text",
	},
	{ --[7]
		header = T"Stop",
		name = "stop",
		param = "stop",
		type = "text",
	},
	{ --[8]
		header = T"Restart",
		name = "restart",
		param = "restart",
		type = "text",
	}
}

local getargs = ngx.req.get_uri_args()
local sortcol
if (getargs.sort) then
  sortcol = getargs.sort:untaint()
else
  sortcol = "name"
end

local init_options = {
	canEdit = false,
	canAdd = false,
	canDelete = false,
	tableid = "inittable",
	basepath = "rpc.gui.init.files.",
	sorted = sortcol,
}

local init_filter = function(data)

	if data.start_order == "" then
		return false
	end

	local function genAttrib(type)
		return {
			button = {
			  class = "btn-"..type,
			  ["data-name"] = data.name
			}
		}
	end

	if data.active == "1" then
		data.toggle = ui_helper.createSimpleButton(T"Disable","refresh",genAttrib("disable"))
	else
		data.toggle = ui_helper.createSimpleButton(T"Enable","refresh",genAttrib("enable"))
	end
	data.restart = ui_helper.createSimpleButton(T"Restart","refresh",genAttrib("restart"))
	data.start = ui_helper.createSimpleButton(T"Start","refresh",genAttrib("start"))
	data.stop = ui_helper.createSimpleButton(T"Stop","refresh",genAttrib("stop"))

	return true
end

local init_data, sipnet_helpmsg = post_helper.handleTableQuery(init_columns, init_options, init_filter)

  ngx.print('\
');  ngx.print(ui_helper.createHeader(T"Management", false, true) ); ngx.print('\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-management.lp")
  ngx.print('\
');
-- dummy form so that refresh button knows which page to reload, keep it
  ngx.print('\
  <form class="form-horizontal" method="post" action="modals/system-init-modal.lp?sort=');  ngx.print(sortcol); ngx.print('">\
  </form>\
  <fieldset id="system-init-tab">\
	<legend>');  ngx.print( T"Init Management" ); ngx.print('</legend>')

	local alert_warn = {
		alert = {
			class = "alert alert-warn"
		},
	}

	ngx.print(ui_helper.createAlertBlock(T"Do NOT randomly disable or restart Init Processes. Some are crucial for the correct booting of the device.",alert_warn))
	ngx.print(ui_helper.createTable(init_columns, init_data, init_options))

	ngx.print('\
  </fieldset>\
  </div>\
  <script>\
  var hdrs = document.getElementsByTagName("th");\
  for(var i=0;i<hdrs.length;i++){\
    var text=hdrs[i].innerText;\
    var sort="";\
    switch(text){\
      case "Name":sort="name";break;\
      case "Start Order":sort="start_order";break;\
      default:sort="";break;\
    }\
    if (sort!="") {\
      hdrs[i].innerHTML=\'<a href="#" data-remote="modals/system-init-modal.lp?sort=\'+sort+\'">\'+text+\'</a>\';\
      hdrs[i].setAttribute("class", "nav");\
    }\
  }\
  function postAction(action) {\
		var t = $(".modal form"), e = t.serializeArray();\
		e.push({\
			name: "action",\
			value: action\
		}, {\
			name: "fromModal",\
			value: "YES"\
		}, {\
			name: "CSRFtoken",\
			value: $("meta[name=CSRFtoken]").attr("content")\
		});\
		tch.loadModal(t = t.attr("action"), e, function () {\
			var t = $(".error");\
			0 < t.length && ($("#modal-no-change").hide(), $("#modal-changes").show());\
			var e = $(".modal-action-advanced:first").is(":visible");\
			0 < t.closest(".advanced").length && !e && r(),\
			$(\'.error input:not([type="hidden"])\').first().trigger("focus")\
		})\
		return false;\
	}\
	$(".btn-disable").on("click", function(){\
		postAction("disable_"+$(this).data("name"));\
	});\
	$(".btn-enable").on("click", function(){\
		postAction("enable_"+$(this).data("name"));\
	});\
	$(".btn-start").on("click", function(){\
		postAction("start_"+$(this).data("name"));\
	});\
	$(".btn-stop").on("click", function(){\
		postAction("stop_"+$(this).data("name"));\
	});\
	$(".btn-restart").on("click", function(){\
		postAction("restart_"+$(this).data("name"));\
	});\
  </script>\
');  ngx.print(ui_helper.createFooter() ); ngx.print('\
');
