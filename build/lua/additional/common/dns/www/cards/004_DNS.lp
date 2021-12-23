--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
local modalPath = "/modals/dns-modal.lp"

if session:hasAccess(modalPath) then
  local proxy = require("datamodel")
  local adguard = proxy.get("rpc.gui.init.files.@AdGuardHome.active")

  if not adguard or adguard[1].value ~= "1" then
    local dyntab_helper = require("web.dyntab_helper")
    local imh = require("internetmode_helper")
    local mode_active = dyntab_helper.process(imh).current.name
    if mode_active == "" then
      for _,v in ipairs(imh) do
        if v.default == true then
          mode_active = v.name
          break
        end
      end
    end

    if mode_active ~= "bridge" then
      local dnsmasq_enabled = "0"
      local dnsmasq_status = "Custom DNS disabled"
      local dns_servers = {}
      local rewrites_count = 0
      local noresolv = "0"

      ---@diagnostic disable-next-line: undefined-field
      local untaint = string.untaint
      local format,match = string.format,string.match
      local tinsert = table.insert
      local split = require("split").split

      for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.",true)) do
        local path = dnsmidx.path
        for _,dnsmif in pairs(proxy.get(path.."interface.")) do
          if dnsmif.value == "lan" then
            noresolv = proxy.get(path.."noresolv")[1].value
            local disabled = proxy.get(path.."disabled")
            if not disabled or disabled[1].value == "0" then
              dnsmasq_enabled = "1"
              dnsmasq_status = "Custom DNS enabled"
              local addresses = proxy.getPN(path.."address.",true)
              rewrites_count = #addresses
              local at = 1
              for _,dnsmsrvr in pairs(proxy.get(path.."server.")) do
                local server = untaint(dnsmsrvr.value)
                local domain,ip = match(server,"/([^/]+)/(.+)")
                if not domain or not ip then
                  tinsert(dns_servers,at,server)
                else
                  tinsert(dns_servers,1,format("%s&rarr;%s",domain,ip))
                  at = at + 1
                end
              end
            end
            break
          end
        end
      end

      if #dns_servers == 0 or noresolv ~= "1" then
        if dnsmasq_enabled == "1" and #dns_servers == 0 then
          dnsmasq_enabled = "0"
          dnsmasq_status = "Custom DNS not configured"
        end
        local interface_pn = proxy.getPN("rpc.network.interface.",true)
        local dnsservers_paths = {}
        for _,v in pairs(interface_pn or {}) do
          if v.path then
            local ifconfig = proxy.get(v.path.."type",v.path.."available")
            if ifconfig[1].value == "wan" and ifconfig[2].value == "1" then
              dnsservers_paths[#dnsservers_paths + 1] = v.path.."dnsservers"
            end
          end
        end
        local dns_rpc_content = proxy.get(unpack(dnsservers_paths))
        for _,v in pairs (dns_rpc_content or {}) do
          if v and v.value ~= "" then
            for _,server in pairs(split(untaint(v.value),",")) do
              dns_servers[#dns_servers+1] = server
            end
          end
        end
      end

      local ui_helper = require("web.ui_helper")

      ngx.print('\
      <div class="span3">\
        <div class="smallcard">\
          ');  ngx.print( ui_helper.createCardHeader(T"DNS",modalPath) ); ngx.print('\
          <div class="content card_bg mirror" data-bg-text="&#xf002">\
            ')
            ngx.print(ui_helper.createSimpleLight(dnsmasq_enabled,dnsmasq_status))
            ngx.print('\
            <span class="simple-desc">\
              <i>&nbsp</i>\
              ');
              ngx.print(format(N('<strong>%d</strong><span> DNS Server:</span>','<strong>%d</strong><span> DNS Servers:</span>',#dns_servers),#dns_servers))
              ngx.print('\
            </span>\
            <p class="subinfos" style="letter-spacing:-1px;font-size:12px;font-weight:bold;">\
              ');
              local max_show = 4
              for i,server in ipairs(dns_servers) do
                if i > 1 and i <= max_show then
                  ngx.print('<br>')
                end
                if i <= max_show then
                  ngx.print(server)
                end
              end
              if #dns_servers > max_show then
                ngx.print(T"...")
              end
              ngx.print('\
            </p>\
            <span class="simple-desc">\
              <i>&nbsp</i>\
              ');
              ngx.print(format(N('<strong>%d</strong><span> DNS Rewrite</span>','<strong>%d</strong><span> DNS Rewrites</span>',rewrites_count),rewrites_count))
              ngx.print('\
            </span>\
          </div>\
        </div>\
      </div>\
      ');
    end
  end
end