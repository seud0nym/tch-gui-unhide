--pretranslated: do not change this file

gettext.textdomain('webui-core')

local lp = require("web.lp")
local content_helper = require("web.content_helper")

    local stats = {
		wansensing_state = "uci.wansensing.global.l2type",
    }
    content_helper.getExactContent(stats)
	
	lp.setpath("/www/snippets/")
	if stats.wansensing_state:match("DSL") then
		lp.include("broadband-xdsl.lp")
	elseif stats.wansensing_state:match("ETH") then
		lp.include("broadband-ethernet.lp")
	end
    ngx.print('\
');