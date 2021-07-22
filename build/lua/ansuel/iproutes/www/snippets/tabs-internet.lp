--pretranslated: do not change this file

gettext.textdomain('webui-core')
local proxy = require("datamodel")

local items = {
  { "iproutes-modal.lp", T"IP Routes" },
  { "bridge-grouping-modal.lp", T"Bridges" },
  { "mwan-modal.lp", T"MWAN Policy Routing" },
}

require("sharedModalTabHelper").createModalTab(items)
