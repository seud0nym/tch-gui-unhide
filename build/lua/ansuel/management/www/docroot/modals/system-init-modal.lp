--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local toupper = string.upper

local function sendcmd(cmd,service)
  ngx.log(ngx.INFO, "Calling "..service.." "..cmd)
	proxy.set("rpc.gui.init."..cmd, service)
  proxy.apply()
  if cmd == "disable" then
    return "#"..service.."-enable"
  elseif cmd == "enable" then  
    return "#"..service.."-disable"
  end
  return "#"..service.."-"..cmd
end

local scrollto
if ngx.req.get_method() == "POST" then
	local action = string.untaint(ngx.req.get_post_args().action)
	if action:match("restart") then
		local name = action:gsub("restart_","")
		scrollto = sendcmd("restart",name)
	elseif action:match("start") then
		local name = action:gsub("start_","")
		scrollto = sendcmd("start",name)
	elseif action:match("stop") then
		local name = action:gsub("stop_","")
		scrollto = sendcmd("stop",name)
	elseif action:match("enable") then
		local name = action:gsub("enable_","")
		scrollto = sendcmd("enable",name)
	elseif action:match("disable") then
		local name = action:gsub("disable_","")
		scrollto = sendcmd("disable",name)
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
	sorted = function(a, b)
  	return toupper(a[sortcol] or "") < toupper(b[sortcol] or "")
	end,
}

local init_filter = function(data)
	if not data.name or data.start_order == "" then
		return false
	end

	local function genAttrib(type)
		return {
			button = {
        id = data.name.."-"..type,
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
  <fieldset id="system-init-tab">\
	<legend>');  ngx.print( T"Init Management" ); ngx.print('</legend>')
	local alert_warn = {
		alert = {
			class = "alert alert-warn"
		},
	}
	ngx.print(ui_helper.createAlertBlock(T"Do NOT randomly disable or restart Init Processes. Some are crucial for the correct booting of the device.",alert_warn))
    local filterclass = {
        input = {
            id = "searchInput",
        }
    }
    local html = {}
    html[#html+1] = "<div class='control-group'><label class='control-label'>Filter</label><div class='controls'>"
    html[#html+1] = "<input data-type='search' name='Filter' class='edit-input span3 no-save' placeholder='Search For Services...' id='searchInput' type='text'>"
    html[#html+1] = "</div></div>"
    ngx.print(html)
	ngx.print(ui_helper.createTable(init_columns, init_data, init_options))
	ngx.print('\
  </fieldset>\
  </form>\
  </div>\
  <script>\
  ')
  if scrollto then
    ngx.print('$("', scrollto,'")[0].scrollIntoView();')
  end
  ngx.print('\
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
    var busy_msg = $(".loading-wrapper");\
    busy_msg.removeClass("hide");\
    busy_msg[0].scrollIntoView();\
    $(".modal-body").scrollLeft(0);\
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
  (function() {\
    $("#searchInput").keyup(function () {\
      var data = this.value.toUpperCase().split(" ");\
      var jo = $("#inittable tbody").find("tr");\
      if (this.value == "") {\
          jo.show();\
          return;\
      }\
      jo.hide();\
      jo.filter(function (i, v) {\
          var $t = $(this);\
          for (var d = 0; d < data.length; ++d) {\
              if ($t.text().toUpperCase().indexOf(data[d]) > -1) {\
                  return true;\
              }\
          }\
          return false;\
      })\
      .show();\
    }).focus(function () {\
      this.value = "";\
      $(this).css({\
          "color": "black"\
      });\
      $(this).unbind(\'focus\');\
    }).css({\
      "color": "#C0C0C0"\
    });\
  }());\
  </script>\
');  ngx.print(ui_helper.createFooter() ); ngx.print('\
');
