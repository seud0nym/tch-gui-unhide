--pretranslated: do not change this file

gettext.textdomain('webui-core')

local items = {
  { "iproutes-modal.lp", T"IP Routing" },
  { "bridge-grouping-modal.lp", T"Bridges" },
}

require("sharedModalTabHelper").createModalTab(items)
