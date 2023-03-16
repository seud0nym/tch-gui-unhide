--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-voice')

local tinsert = table.insert

local proxy = require ("datamodel")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local content_helper = require("web.content_helper")
local format = string.format

local function vAccCode(value)
    local errmsg = T"Access code must composed of 4 digits"

    if value == nil or #value == 0 then
        -- empty Access code just means that we don't want to set one
        return nil,errmsg
    end

    local acc_code = value:match("^(%d%d%d%d)$")
    if acc_code then
        return true
    end
    return nil, errmsg
end

local function validatePairTime(value)
    local ermsg = T"Pairing window value is invalid"
    if value == nil or #value == 0 then
       return nil,ermsg
    end
    local pairing_window = value:match("^(%d+)$")
        if pairing_window then
                return true
        end
        return nil, ermsg
end
local mapValid = {
    pairTime = validatePairTime,
    accCode = vAccCode,
    baseName = post_helper.validateStringIsBaseName
}

local mapParams = {
    pairTime = "uci.mmpbxbrcmdectdev.dect_base.pairing_window",
    accCode = "uci.mmpbxbrcmdectdev.dect_base.access_code",
    baseName = "uci.mmpbxbrcmdectdev.dect_base.user_friendly_name"
}

local content, helpmsg = post_helper.handleQuery(mapParams, mapValid)
if not helpmsg then
    helpmsg = {}
end

content["handset_id"] = "all"

-- Dect Devices
local dectdev_columms = {
    {
        header = T"Name",
        name = "name",
        param = "FriendlyName",
        type = "text",
        readonly = true,
        attr = { input = { class="span2" } },
    },
    {
        header = T"Handset ID",
        name = "ipui",
        param = "IPUI",
        type = "text",
        readonly = true,
        attr = { input = { class="span2" } },
    },
    {
        header = T"State",
        name = "locationStatus",
        param = "LocationStatus",
        type = "text",
        readonly = true,
        attr = { input = { class="span2" } },
    },
}

local dectdev_options = {
    tableid = "dectDev",
    basepath = "sys.dect.dev.",
    canAdd = false,
    canEdit = false,
    canDelete = false,
    sorted = "FriendlyName",
}

local dectdev_data, dectdev_helpmsg = post_helper.handleTableQuery(dectdev_columms, dectdev_options, nil, nil, nil)

local function findHandsetID(handset_name)
    local devName = ""

    for _,v in ipairs(proxy.getPN("uci.mmpbxbrcmdectdev.device.",true)) do -- get the path
        devName = format("%s", proxy.get(v["path"].."user_friendly_name")[1].value)
        if handset_name == devName then
            local devId = tonumber(string.match(devName, "%d+")-1)
            return format("dect_dev_%d",devId)
        end
    end
end

-- Build the device selection list
local dect_located_list = {}

tinsert(dect_located_list, {"all", T"ALL"})
for _,v in ipairs(dectdev_data) do
    local disp_val = format("%s", v[1])
    local ipui = format("%s", v[2])

    if ipui ~= "0000000000" then
        tinsert(dect_located_list, {disp_val, disp_val})
    end
end

-- triggered only on a POST with action set pairing
if ngx.req.get_method() == "POST" then
    local args = ngx.req.get_post_args()
    local action = args["action"]

    if action == "paging_handset" then
        --ngx.log (ngx.ERR, "Check Paging Handset - ", "action")
        proxy.set("rpc.mmpbxbrcmdect.paging","toggle")
    end
    if action == "pairing_handset" then
        ngx.header.content_type = "application/json"

        proxy.set("rpc.mmpbxbrcmdect.registration","open")

        ngx.print('{ "success":"true" }')
        ngx.exit(ngx.HTTP_OK)
    end
    -- Deregister one or all DECT handset
    if action == "unreg_handset" then
        local handset_id = args["handset_id"]
        local handset_name = "all"

        if handset_id ~= "all" then
            content["handset_id"]=format("%s", handset_id)
            handset_name = findHandsetID(handset_id)
        end
        --ngx.log (ngx.ERR, "Unregister Handset - ", handset_name)
        proxy.set("rpc.mmpbxbrcmdect.registration.clear",handset_name)

        ngx.header.content_type = "application/json"
        ngx.print('{ "success":"true" }')
        ngx.exit(ngx.HTTP_OK)
    end
    if action == "disable_emission" then
        proxy.set("rpc.mmpbx.dectemission.state","0")
        proxy.apply()
    end
    if action == "enable_emission" then
        proxy.set("rpc.mmpbx.dectemission.state","1")
        proxy.apply()
    end
elseif ngx.req.get_method() == "GET" then
    local args = ngx.req.get_uri_args()
    local action = args["action"]
    if action == "pairing_state" then
        local content = {
            open = "rpc.mmpbxbrcmdect.registration.open"
        }
        content_helper.getExactContent(content)

        --ngx.log (ngx.ERR, "Check GET Pairing Handset - Return value: ", "content.open" )
        ngx.header.content_type = "application/json"
        if content.open == "1" then
            ngx.print('{ "success":"true" }')
        else
            ngx.print('{ "success":"false" }')
        end
        ngx.exit(ngx.HTTP_OK)
    end
end

-- Retrieving paging state
local rpc_content = {
    paging_state = "rpc.mmpbxbrcmdect.paging.alerting",
    emission_state = "rpc.mmpbx.dectemission.state"
}
content_helper.getExactContent(rpc_content)

local autorefresh = nil
local paging_state = format("%s", rpc_content.paging_state)

if paging_state == "1" then
    autorefresh = 2
end

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
    uri = uri .. "?" .. ngx.var.args
end

ngx.print(ui_helper.createHeader(T"Telephony", false, true, autorefresh),'\
<div class="modal-body update">')
    local lp = require("web.lp")
    lp.setpath("/www/snippets/")
    lp.include("tabs-voice.lp")
ngx.print('\
  <form class="form-horizontal" method="post" action="',uri,'">\
    <fieldset>\
      <legend>',T"DECT Emission",'</legend>')
        if rpc_content.emission_state == "1" then
            local dis_emission_class = {
                button = {
                  ["data-name"] = "action",
                  ["data-value"] = "disable_emission",
                }
            }
            ngx.print(ui_helper.createButton (T"Emission Mode",T"Disable","icon-ban-circle icon-large red",dis_emission_class))
        else
            local en_emission_class = {
                button = {
                  ["data-name"] = "action",
                  ["data-value"] = "enable_emission",
                }
            }
            ngx.print(ui_helper.createButton (T"Emission Mode", T"Enable", "icon-bolt", en_emission_class))
        end
        ngx.print ('\
    </fieldset>\
    <fieldset id ="mmpbx-dect-tab">\
      <legend>',T"Pairing",'</legend>')
        local base_name_class = {
            input = {
                pattern= "^([a-zA-Z]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}$",
                maxlength= "16",
                class= "span4",
            }
        }
        ngx.print(ui_helper.createInputText(T"Base Name", "baseName", content["baseName"], base_name_class, helpmsg["baseName"]))

        local access_code_class = {
            input = {
                pattern= "^([0-9]{4})$",
                maxlength= "4",
                class= "span4",
            }
        }
        ngx.print(ui_helper.createInputText(T"Access Code", "accCode", content["accCode"], access_code_class, helpmsg["accCode"]))

        local pairing_window_class = {
            input = {
                pattern= "^([0-9]{1,4})$",
                maxlength= "4",
                class= "span4",
            }
        }
        ngx.print(ui_helper.createInputText(T"Pairing Window Time in Sec", "pairTime", content["pairTime"], pairing_window_class, helpmsg["pairTime"]))

        local pairing_class = {
            group = {
                class = "pairing-class"
            },
            button = {
                id = "btn-pairing",
                class = "custom-handler"
            }
        }
        ngx.print(ui_helper.createButton (T"Pairing Handset", T"Start", "icon-refresh", pairing_class))

        local pairing = {
            alert = {
                class = "alert hide",
                id = "pairing_msg"
            }
        }
        ngx.print(ui_helper.createAlertBlock(T"Pairing in progress, please wait...", pairing))
     ngx.print('\
    </fieldset>\
    <fieldset>\
      <legend>',T"Paging",'</legend>')
        local page_class = {
            button = {
                ["data-name"] = "action",
                ["data-value"] = "paging_handset",
            }
        }
        if paging_state == "0" then
            ngx.print(ui_helper.createButton (T"Paging Handset", T"Start", "icon-bolt", page_class))
        else
            ngx.print(ui_helper.createButton (T"Paging Handset", T"Stop", "icon-ban-circle icon-large red", page_class))
        end
     ngx.print('\
    </fieldset>\
    <fieldset>\
      <legend>',T"Device List",'</legend>',
        ui_helper.createTable(dectdev_columms, dectdev_data, dectdev_options, nil, dectdev_helpmsg))
        local handset_class = {
            group = {
                class = "",
            },
            select = {
                class = "no-save",
                id = "handset_id"
            },
            button = {
                id = "btn-unreg",
                class = "custom-handler"
            }
        }
        ngx.print(ui_helper.createInputSelectWithButton(T"Handset Name", "handset_id", dect_located_list, content["handset_id"], T"Unregister", "icon-trash large red", handset_class),'\
    </fieldset>\
  </form>\
</div>\
<div id="spinner" class="spinner hide">\
<img id="img-spinner" src="../img/spinner.gif" alt="Pairing" style="margin-left: 10px; margin-right: 10px; margin-top: 10px; margin-bottom: 10px; width:128px; height:128px"/>\
</div>\
<script>\
(function() {\
    var target = $(".modal form").attr("action");\
    var modalbody = $(".modal-body");\
    var scrolltop = $(".modal-body").scrollTop();\
    var checkDelay = 3000;\
    var refreshTimeOut = 5000;\
    var time_elapsed = 0;\
    var pairtime = parseInt(document.getElementsByName("pairTime")[0].value) * 1000;\
    var max_timer = pairtime + checkDelay + 1000;\
    var _this;\
    function wait_to_registered() {\
        $.ajax({ url: target, data: "action=pairing_state", timeout: refreshTimeOut, dataType: "json" })\
        .done(function(data) {\
            if (data.success == "true") {\
                time_elapsed = time_elapsed + checkDelay;\
                if (time_elapsed < max_timer) {\
                    window.setTimeout(wait_to_registered, checkDelay);\
                }\
            }\
            else {\
                var msg_dst = _this;\
                var busy_msg = $("#spinner");\
                var in_progress = $("#pairing_msg");\
                msg_dst.after(busy_msg);\
                in_progress.addClass("hide");\
                busy_msg.addClass("hide");\
                busy_msg[0].scrollIntoView();\
                tch.loadModal(target, function () {\
                    $(".modal-body").scrollTop(scrolltop);\
                });\
            }\
        });\
    }\
    $("#btn-pairing").click(function() {\
        var msg_dst = $(this);\
        var busy_msg = $("#spinner");\
        var in_progress = $("#pairing_msg");\
        _this = $(this);\
        msg_dst.after(busy_msg);\
        in_progress.removeClass("hide");\
        busy_msg.removeClass("hide");\
        busy_msg[0].scrollIntoView();\
        $.post(\
            target,\
            { action: "pairing_handset", CSRFtoken: $("meta[name=CSRFtoken]").attr("content") },\
            wait_to_registered,\
            "json"\
        );\
        return false;\
    });\
    function wait_to_unregisterd() {\
        tch.loadModal(target, function () {\
            $(".modal-body").scrollTop(scrolltop);\
        });\
    };\
    $("#btn-unreg").click(function() {\
        if (document.getElementById("handset_id").value == "all") {\
            var r = confirm("Are you sure you want to Unregister all the Dect Handset?");\
            if (r == false) {\
                return false;\
            }\
        }\
        $.post(\
            target,\
            {\
                action: "unreg_handset",\
                handset_id: document.getElementById("handset_id").value,\
                CSRFtoken: $("meta[name=CSRFtoken]").attr("content")\
            },\
            wait_to_unregisterd,\
            "json"\
        );\
        return false;\
    });\
}());\
</script>\
',ui_helper.createFooter())
