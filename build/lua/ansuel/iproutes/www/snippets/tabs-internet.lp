--pretranslated: do not change this file

gettext.textdomain('webui-core')
local proxy = require("datamodel")

local items = {
  { "iproutes-modal.lp", T"IP Routing" },
  { "dns-modal.lp", T"DNS" },
  { "bridge-grouping-modal.lp", T"Bridges" },
}

require("sharedModalTabHelper").createModalTab(items)
