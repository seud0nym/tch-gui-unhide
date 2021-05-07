gettext.textdomain('webui-core')
local proxy = require("datamodel")
local ifnames = proxy.get("uci.network.interface.@lan.ifname")[1].value
local wan_ifname = proxy.get("uci.network.interface.@wan.ifname")
local gsub = string.gsub

if wan_ifname then
    wan_ifname = wan_ifname[1].value
else
    wan_ifname = ""
end

return {
    {
        name = "dhcp",
        default = true,
        description = T"DHCP routed mode",
        view = "internet-dhcp-routed.lp",
        card = "003_internet_dhcp_routed.lp",
        check = {
            { "uci.network.interface.@wan.proto", "^dhcp$"},
        },
        operations = {
            { "uci.network.interface.@wan.proto", "dhcp"},
            { "uci.network.interface.@wan6.proto", "dhcpv6"},
            { "uci.network.interface.@wan6.ifname", wan_ifname},
            { "uci.network.config.wan_mode", "dhcp"},
            { "uci.network.interface.@lan.ifname", gsub(gsub(ifnames, wan_ifname, ""), "%s$", "")},
        },
    },
    {
        name = "pppoe",
        default = false,
        description = T"PPPoE routed",
        view = "internet-pppoe-routed.lp",
        card = "003_internet_pppoe_routed.lp",
        check = {
            { "uci.network.interface.@wan.proto", "^pppoe$"},
        },
        operations = {
            { "uci.network.interface.@wan.proto", "pppoe"},
            { "uci.network.config.wan_mode", "pppoe"},
            { "uci.network.interface.@lan.ifname", gsub(gsub(ifnames, wan_ifname, ""), "%s$", "")},
        },
    },
    {
        name = "pppoa",
        default = false,
        description = T"PPPoA routed",
        view = "internet-pppoa-routed.lp",
        card = "003_internet_pppoe_routed.lp",
        check = {
            { "uci.network.interface.@wan.proto", "^pppoa$"},
        },
        operations = {
            { "uci.network.interface.@wan.proto", "pppoa"},
            { "uci.network.config.wan_mode", "pppoa"},
            { "uci.network.interface.@lan.ifname", gsub(gsub(ifnames, wan_ifname, ""), "%s$", "")},
        },
    },
    {
        name = "static",
        default = false,
        description = T"Static IP mode",
        view = "internet-static-routed.lp",
        card = "003_internet_static_routed.lp",
        check = {
            { "uci.network.interface.@wan.proto", "^static$"},
        },
        operations = {
            { "uci.network.interface.@wan.proto", "static"},
            { "uci.network.interface.@wan6.proto", "static"},
            { "uci.network.interface.@wan6.ifname", wan_ifname},
            { "uci.network.config.wan_mode", "static"},
            { "uci.network.interface.@lan.ifname", gsub(gsub(ifnames, wan_ifname, ""), "%s$", "")},
        },
    },
    {
        name = "bridge",
        default = false,
        description = T"Bridge mode",
        view = "internet-bridged.lp",
        card = "003_internet_bridged.lp",
        check = {
            { "uci.network.config.wan_mode", "^bridge$"}
        },
        operations = {
            { "uci.network.config.wan_mode", "bridge"},
            { "uci.network.interface.@lan.ifname", ifnames ..' '.. wan_ifname},
        },
    },
}
