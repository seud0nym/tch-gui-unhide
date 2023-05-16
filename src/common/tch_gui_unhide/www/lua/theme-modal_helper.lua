local proxy = require("datamodel")
local string = string
local gsub = string.gsub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local name = proxy.get("uci.env.var.variant_friendly_name")
local variant
if not name then
  name = proxy.get("env.var.prod_friendly_name")
  variant = gsub(untaint(name[1].value),"Technicolor ","")
else
  variant = gsub(untaint(name[1].value),"TLS","")
end

local M = {}

M.theme_names = {
    {"light",T"Light"},
    {"night",T"Night"},
    {"classic",T"Classic"},
    {"telstra",T"Classic (Telstra Branding)"},
    {"telstramodern",T"Modern (Telstra Branding)"},
}
M.theme_colours = {
    {"blue",T"Blue"},
    {"green",T"Green"},
    {"orange",T"Orange"},
    {"purple",T"Purple"},
    {"red",T"Red"},
    {"monochrome",T"Monochrome"},
    {"MONOCHROME",T"Monochrome (with Monochrome charts)"},
}
M.card_icons_options = {
    {"visible",T"Visible"},
    {"hidden",T"Hidden"},
}
M.times = {
  {"0:0",T"Midnight"},
  {"0:15",T"12:15am"},
  {"0:30",T"12:30am"},
  {"0:45",T"12:45am"},
  {"1:0",T"1:00am"},
  {"1:15",T"1:15am"},
  {"1:30",T"1:30am"},
  {"1:45",T"1:45am"},
  {"2:0",T"2:00am"},
  {"2:15",T"2:15am"},
  {"2:30",T"2:30am"},
  {"2:45",T"2:45am"},
  {"3:0",T"3:00am"},
  {"3:15",T"3:15am"},
  {"3:30",T"3:30am"},
  {"3:45",T"3:45am"},
  {"4:0",T"4:00am"},
  {"4:15",T"4:15am"},
  {"4:30",T"4:30am"},
  {"4:45",T"4:45am"},
  {"5:0",T"5:00am"},
  {"5:15",T"5:15am"},
  {"5:30",T"5:30am"},
  {"5:45",T"5:45am"},
  {"6:0",T"6:00am"},
  {"6:15",T"6:15am"},
  {"6:30",T"6:30am"},
  {"6:45",T"6:45am"},
  {"7:0",T"7:00am"},
  {"7:15",T"7:15am"},
  {"7:30",T"7:30am"},
  {"7:45",T"7:45am"},
  {"8:0",T"8:00am"},
  {"8:15",T"8:15am"},
  {"8:30",T"8:30am"},
  {"8:45",T"8:45am"},
  {"9:0",T"9:00am"},
  {"9:15",T"9:15am"},
  {"9:30",T"9:30am"},
  {"9:45",T"9:45am"},
  {"10:0",T"10:00am"},
  {"10:15",T"10:15am"},
  {"10:30",T"10:30am"},
  {"10:45",T"10:45am"},
  {"11:0",T"11:00am"},
  {"11:15",T"11:15am"},
  {"11:30",T"11:30am"},
  {"11:45",T"11:45am"},
  {"12:0",T"12:00pm"},
  {"12:15",T"12:15pm"},
  {"12:30",T"12:30pm"},
  {"12:45",T"12:45pm"},
  {"13:0",T"1:00pm"},
  {"13:15",T"1:15pm"},
  {"13:30",T"1:30pm"},
  {"13:45",T"1:45pm"},
  {"14:0",T"2:00pm"},
  {"14:15",T"2:15pm"},
  {"14:30",T"2:30pm"},
  {"14:45",T"2:45pm"},
  {"15:0",T"3:00pm"},
  {"15:15",T"3:15pm"},
  {"15:30",T"3:30pm"},
  {"15:45",T"3:45pm"},
  {"16:0",T"4:00pm"},
  {"16:15",T"4:15pm"},
  {"16:30",T"4:30pm"},
  {"16:45",T"4:45pm"},
  {"17:0",T"5:00pm"},
  {"17:15",T"5:15pm"},
  {"17:30",T"5:30pm"},
  {"17:45",T"5:45pm"},
  {"18:0",T"6:00pm"},
  {"18:15",T"6:15pm"},
  {"18:30",T"6:30pm"},
  {"18:45",T"6:45pm"},
  {"19:0",T"7:00pm"},
  {"19:15",T"7:15pm"},
  {"19:30",T"7:30pm"},
  {"19:45",T"7:45pm"},
  {"20:0",T"8:00pm"},
  {"20:15",T"8:15pm"},
  {"20:30",T"8:30pm"},
  {"20:45",T"8:45pm"},
  {"21:0",T"9:00pm"},
  {"21:15",T"9:15pm"},
  {"21:30",T"9:30pm"},
  {"21:45",T"9:45pm"},
  {"22:0",T"10:00pm"},
  {"22:15",T"10:15pm"},
  {"22:30",T"10:30pm"},
  {"22:45",T"10:45pm"},
  {"23:0",T"11:00pm"},
  {"23:15",T"11:15pm"},
  {"23:30",T"11:30pm"},
  {"23:45",T"11:45pm"},
}
M.card_titles = {
  adblock = "Adblock",
  broadband = "Broadband",
  contentsharing = "Content Sharing",
  cwmpconf = "CWMP",
  Devices = "Devices",
  devicesecurity = "Device Security",
  diagnostics = "Diagnostics",
  eco = "Eco Settings",
  firewall = "Firewall",
  fon = "Telstra AIR",
  gateway = variant,
  internet = "Internet Access",
  iproutes = "IP Routing",
  LAN = "Local Network",
  lte = "Mobile",
  natalghelper = "NAT Helpers",
  opkg = "Packages",
  parental = "Parental Controls",
  printersharing = "Printer Sharing",
  qos = "QoS",
  relaysetup = "Relay Setup",
  speedservice = "Speed Test",
  system = "System Extras",
  telephony = "Telephony",
  tod = "Time of Day",
  usermgr = "Management",
  WANDown = "WAN Download",
  wanservices = "WAN Services",
  WANUp = "WAN Upload",
  wireless = "Wi-Fi",
  wireless_boosters = "Wi-Fi Boosters",
  xdsl = "xDSL Config",
}

return M