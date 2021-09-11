local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local untaint = string.untaint

local M = {}

function M.getIPsecCardHTML()
  local content = {
    associations = "rpc.gui.ipsec.associations",
    firewall = "uci.firewall.include.@ipsec_responder.enabled",
    v1_psk_xauth_auto = "rpc.gui.ipsec.conn.@tgu-responder-ikev1-psk-xauth.auto",
    v2_mschapv2_auto = "rpc.gui.ipsec.conn.@tgu-responder-ikev2-mschapv2.auto",
    v2_psk_auto = "rpc.gui.ipsec.conn.@tgu-responder-ikev2-psk.auto",
  }
  content_helper.getExactContent(content)

  local enabled = "1"
  if content.firewall ~= "0" then
    content.firewall = "1"
    content.firewall_text = "Incoming Firewall Ports open"
  else
    content.firewall_text = "Incoming Firewall Ports closed"
    enabled = "2"
  end
  if content.firewall ~= "0" then
    content.firewall = "1"
    content.firewall_text = "Incoming Firewall Ports open"
  else
    content.firewall_text = "Incoming Firewall Ports closed"
  end
  if content.v1_psk_xauth_auto ~= "add" then
    content.v1_psk_xauth_auto = "0"
    content.v1_psk_xauth_text = "IPsec/Xauth/PSK disabled"
  else
    content.v1_psk_xauth_auto = enabled
    content.v1_psk_xauth_text = "IPsec/Xauth/PSK enabled"
  end
  if content.v2_mschapv2_auto ~= "add" then
    content.v2_mschapv2_auto = "0"
    content.v2_mschapv2_text = "IKEv2/EAP-MSCHAPv2 disabled"
  else
    content.v2_mschapv2_auto = enabled
    content.v2_mschapv2_text = "IKEv2/EAP-MSCHAPv2 enabled"
  end
  if content.v2_psk_auto ~= "add" then
    content.v2_psk_auto = "0"
    content.v2_psk_text = "IKEv2/PSK disabled"
  else
    content.v2_psk_auto = enabled
    content.v2_psk_text = "IKEv2/PSK enabled"
  end
  
  local html = {}
  html[#html+1] = ui_helper.createSimpleLight(content.firewall,content.firewall_text)
  html[#html+1] = '<p class="subinfos">'
  html[#html+1] = 'Associations: ' .. untaint(content.associations)
  html[#html+1] = '</p>'
  html[#html+1] = ui_helper.createSimpleLight(content.v2_mschapv2_auto,content.v2_mschapv2_text)
  html[#html+1] = ui_helper.createSimpleLight(content.v2_psk_auto,content.v2_psk_text)
  html[#html+1] = ui_helper.createSimpleLight(content.v1_psk_xauth_auto,content.v1_psk_xauth_text)
  return html
end

return M
