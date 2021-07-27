--pretranslated: do not change this file

gettext.textdomain('webui-core')
local proxy = require("datamodel")
local adguard = proxy.get("rpc.gui.init.files.@AdGuardHome.active")

local items = {
  { "iproutes-modal.lp", T"IP Routing" },
}
if not adguard or adguard[1].value ~= "1" then
  items[#items+1] = { "dns-modal.lp", T"DNS" }
end
items[#items+1] = { "bridge-grouping-modal.lp", T"Bridges" }

require("sharedModalTabHelper").createModalTab(items)
