--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ngx = ngx

local vB = post_helper.validateBoolean

local dos_Params = {
	dos_enable = "uci.dosprotect.globals.enabled",
	dos_rpfilter = "uci.dosprotect.globals.rpfilter",
	dos_ratelimit = "rpc.gui.firewall.dosprotect_ratelimit",
}

local dos_valid = {
	dos_enable = vB,
	dos_rpfilter = vB,
	dos_ratelimit = vB,
}

local dos_content = post_helper.handleQuery(dos_Params,dos_valid)

  ngx.print('\
');  ngx.print(ui_helper.createHeader(T"DoS Protect Option",false,false))   ngx.print('\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-firewall.lp")
  ngx.print('\
  <form class="form-horizontal" method="post" action="modals/dosprotect-modal.lp">\
    ');
      ngx.print(ui_helper.createMessages(message_helper.popMessages()))
      ngx.print('\
    <fieldset>\
      ');
			local html = {}
			html[#html + 1] = '<legend>'..T"Settings"..'</legend>'
			html[#html + 1] = ui_helper.createSwitch(T"Enabled","dos_enable",dos_content["dos_enable"])
			html[#html + 1] = ui_helper.createSwitch(T"Reverse Path Filter<span class='icon-question-sign' title='Drop IPv4 and IPv6 packets whose source address could not be reached back through the interface they arrived on (spoofed sources).'></span>","dos_rpfilter",dos_content["dos_rpfilter"])
			html[#html + 1] = ui_helper.createSwitch(T"Rate Limiting<span class='icon-question-sign' title='Limit packets per source address on every interface, including the LAN (for example UDP above 1000 packets per second, and ICMP above 5 per second). Turn off to use the Reverse Path Filter without limiting LAN traffic.'></span>","dos_ratelimit",dos_content["dos_ratelimit"])
			ngx.print(html)
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
');