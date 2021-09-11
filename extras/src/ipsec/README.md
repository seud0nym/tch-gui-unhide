## tch-gui-unhide-xtra.ipsec
Creates a GUI interface for configuring IKEv1/IKEv2 IPsec VPN using strongswan.
#### Download
https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/extras/tch-gui-unhide-xtra.ipsec
#### Firmware Applicability
For firmware 18.1.c. *only*.
- The firmware 17.2 repository unfortunately does *not* contain the required packages.
- Firmware 20.3.c is missing a required kernel module and connections fail with a received *Protocol not supported (93)* error.
#### Prerequisites 
##### CA Certificates
The System CA Certificates must be updated. You can do this either by (preferably) running the [`update-ca-certificates`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#update-ca-certificates) script, or by manually installing the CA certificates packages using `opkg install ca-certificates ca-bundle`. The `update-ca-certificates` script will install the latest available certificates (and gives you the option to schedule a regular job to update them), whereas the opkg packages may not contain the latest certificates.
##### Dynamic DNS and Server Certificate
If you are setting up an IKE VPN Server (responder), you must have a DNS entry pointing to your IPv4 address, and a Server Certificate validating the domain name. These can be configured on **WAN Services** card. Note that if you have a static IP address and a domain name assigned by your ISP, the domain name still needs to be entered under IPv4 Dynamic DNS, but you do not have to enable the Dynamic DNS Service. The Server Certificate is still required.
##### Package Installation
`opkg install strongswan-default strongswan-mod-eap-mschapv2 strongswan-mod-dhcp strongswan-mod-farp strongswan-mod-openssl`
#### Changes External to the GUI
The installation creates the the following transformer UCI mappings and firewall scripts to support the GUI changes:
- /usr/share/transformer/mappings/rpc/gui.ipsec.map
- /etc/firewall.ipsec.responder
#### Removal Instructions
1. Do **not** delete `tch-gui-unhide-xtra.ipsec`
2. Remove strongswan: `opkg --autoremove --force-depends --force-removal-of-dependent-packages remove strongswan-default strongswan-mod-eap-mschapv2 strongswan-mod-dhcp strongswan-mod-farp strongswan-mod-openssl`
3. Re-run `tch-gui-unhide` to remove the GUI changes, custom configuration and firewall script
4. Now you can delete `tch-gui-unhide-xtra.ipsec`

