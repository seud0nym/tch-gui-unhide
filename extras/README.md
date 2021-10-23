# Extras

This is where extra functionality scripts can be found. These are not incorporated in the main tch-gui-unhide code line, because they rely on additional packages being installed.

Extras scripts that rely on packages to be installed require `opkg` to be configured correctly on your device. See **opkg Configuration** [`below`](https://github.com/seud0nym/tch-gui-unhide/tree/master/extras#opkg-Configuration).

## tch-gui-unhide-xtra.adblock
Creates a GUI interface for the [`Adblock`](https://openwrt.org/packages/pkgdata/adblock) package that allows you to block ads at the router level.
#### Firmware Applicability
Should be applicable to all firmware versions supported by `tch-gui-unhide`, as long as you install the firmware-specific prerequisites.
#### Prerequisites 
##### CA Certificates
The System CA Certificates must be updated. You can do this either by (preferably) running the [`update-ca-certificates`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#update-ca-certificates) script, or by manually installing the CA certificates packages using `opkg install ca-certificates ca-bundle`. The `update-ca-certificates` script will install the latest available certificates (and gives you the option to schedule a regular job to update them), whereas the opkg packages may not contain the latest certificates.
##### adblock
Requires adblock version 3.5 to be installed using the `opkg` command. Different firmware versions have different dependencies:
###### Firmware 20.3.c
`opkg install adblock uclient-fetch`
###### Firmware 18.1.c
`opkg install adblock uclient-fetch libustream-openssl`
###### Firmware 17.2
The version of adblock in the standard 17.2 repository is incompatible with this extra script, but you can manually download and install the Homeware 18 version on the 17.2 firmware:  
`curl -k https://repository.macoers.com/homeware/18/brcm63xx-tch/VANTW/packages/adblock_3.5.5-4_all.ipk -o/tmp/adblock_3.5.5-4_all.ipk`  
`opkg install /tmp/adblock_3.5.5-4_all.ipk`  
`uci set adblock.global.adb_fetchutil='curl'`  
`uci commit adblock`
#### Installation
`./tch-gui-unhide -x adblock`
#### Post-Installation Configuration
- Configure the IPv4 Primary DNS Server under Local Network to the local device (depending on your version of tch-gui-unhide, this will be "RSP/ISP DNS Servers", "Custom WAN DNS Servers", or the device variant (e.g. "DJA0230" or "DJA0231"))
- Leave the IPV4 Secondary DNS Server as blank
- Configure your preferred DNS Servers under Internet Access
- Enable DNS Intercept under Firewall (leave DNS Server Address blank)
#### Removal Instructions
1. Delete `tch-gui-unhide-xtra.adblock`
2. Remove adblock and dependencies: `opkg remove adblock uclient-fetch`
    - If you are on firmware 18.1.c, then also: `opkg remove libustream-openssl`
3. Re-run `tch-gui-unhide` to remove the GUI changes, and the additional transformer mappings

## tch-gui-unhide-xtra.minidlna
Replaces the stock DLNA server management in the GUI so that it supports OpenWRT minidlna.
#### Firmware Applicability
Should be applicable to all firmware versions supported by `tch-gui-unhide`.
#### Prerequisites 
Install minidlna using the opkg command: `opkg install minidlna`
#### Installation
`./tch-gui-unhide -x minidlna`
#### Removal Instructions
1. Do **not** delete `tch-gui-unhide-xtra.minidlna`
2. Remove minidlna: `opkg remove minidlna`
3. Re-run `tch-gui-unhide` to remove the GUI changes, custom configuration and the additional transformer mappings
4. Now you can delete `tch-gui-unhide-xtra.minidlna`

## tch-gui-unhide-xtra.rsyncd
Adds the ability to enable and disable the rsync daemon from the GUI.
- Adds the *home* module to /etc/rsyncd.conf to allow read/write access to the /root directory via rsync (e.g. `rsync 192.168.0.1::home`)
- Adds the *tmp* module to /etc/rsyncd.conf to allow read/write access to the /tmp directory via rsync (e.g. `rsync 192.168.0.1::tmp`)
- Adds the *usb* module to /etc/rsyncd.conf to allow read/write access to the USB device via rsync (e.g. `rsync 192.168.0.1::usb`)
#### Firmware Applicability
Should be applicable to all firmware versions supported by `tch-gui-unhide`.
#### Prerequisites 
Install rsyncd using the opkg command: `opkg install rsync rsyncd`
#### Installation
`./tch-gui-unhide -x rsyncd`
#### Removal Instructions
1. Delete `tch-gui-unhide-xtra.rsyncd`
2. Remove rsyncd: `opkg remove rsync rsyncd`
3. Re-run `tch-gui-unhide` to remove the GUI changes, and the additional transformer mappings

## tch-gui-unhide-xtra.samba36-server
Correctly configures OpenWRT SAMBA 3.6 to provide SMBv2 for Windows 10 inter-operability. This update adds the ability to change the password via the GUI.
#### Firmware Applicability
You should only install SAMBA 3.6 on the 17.2 and 18.1.c firmware. The 20.3.c firmware contains NQE rather than SAMBA, and does not require the SAMBA 3.6 update to upgrade to SMBv2 and inter-operate with Windows 10.
#### Prerequisites
Install SAMBA v3.6 using the opkg command: `opkg --force-overwrite install samba36-server`
#### Installation
`./tch-gui-unhide -x samba36-server`
#### Removal Instructions
1. Do **not** delete tch-gui-unhide-xtra.samba36-server
2. Remove samba36-server: `opkg remove samba36-server`
3. Re-run `tch-gui-unhide` to correctly restore the default version of SAMBA on the device and remove the GUI changes, the custom configuration and the additional transformer mappings
4. Now you can delete `tch-gui-unhide-xtra.samba36-server`

## tch-gui-unhide-xtra.wireguard
Creates a GUI interface for configuring the Wireguard VPN.
#### Firmware Applicability
For firmware 20.3.c. *only*.
- Firmware prior to 20.3.c has not been compiled with TUN support in the kernel, and therefore VPN tunnels cannot be created.
#### Prerequisites
You must install [openwrt-wireguard-go](https://github.com/seud0nym/openwrt-wireguard-go) using this command:  
`curl -skL https://raw.githubusercontent.com/seud0nym/openwrt-wireguard-go/master/install_arm.sh | sh -s --`
##### IPv6 ULA Prefix
If you are using IPv6, you need an IPv6 ULA (Unique Local Addresses) prefix. This can be configured on the **Local Network** card. If you do not configure your own ULA prefix and your network is configured for IPv6, the installation script will create a random one for you.
**IMPORTANT:** Because these devices have no IPv6 NAT capability, IPv6 packets from client devices will *NOT* be sent via a WireGuard tunnel acting as a client to a remote VPN server. IPv6 traffic from the router itself *will* be routed via the tunnel. IPv4 traffic for both the router and client devices *will* be routed via the tunnel.
##### Dynamic DNS
If you are setting up a Wireguard VPN Server and your ISP/RSP provides your IPv4 address via DHCP, you should have a DNS entry pointing to your IPv4 address. This can be configured on the **WAN Services** card.  
If you have a static IP address and a domain name assigned by your ISP, the domain name can still be entered under IPv4 Dynamic DNS, but you do not have to *enable* the Dynamic DNS Service.
#### Installation
`./tch-gui-unhide -x wireguard`
#### Removal Instructions
1. Do **not** delete `tch-gui-unhide-xtra.wireguard`
2. Uninstall openwrt-wireguard-go: `wg --uninstall`
3. Re-run `tch-gui-unhide` to remove the GUI changes, custom configuration and firewall script
4. Now you can delete `tch-gui-unhide-xtra.wireguard`

# How to download and execute these scripts

### Using the tch-gui-unhide -x option
Use the `-x` option on the `tch-gui-unhide` command to download and apply the scripts. You can specify mulitple `-x` options to install multiple extras scripts.

### Manual Download
Download the scripts that you wish to execute into the same directory as `tch-gui-unhide`.

**NOTE: Replace `<scriptname>` with the name of the script you wish to download.**

Execute these commands on your device via a PuTTY session or equivalent (an active WAN/Internet connection is required):
```
curl -kLO https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/extras/<scriptname> 
```

Alternatively, download the script manually and load it up to your device using WinSCP or equivalent.

After you have the script on your device, you may need to make it executable, which is done with this command (assuming you are in the same directory as the script):
```
chmod +x <scriptname>
```

These scripts will be automatically run by `tch-gui-unhide` if they exist in the directory when it is executed. They are **NOT** intended to be executed outside of `tch-gui-unhide`.

# opkg Configuration
Once you have correctly configured `opkg` using one of the following methods, you need to run the `opkg update` command before installing packages.

## Packages Card in User Interface (after tch-gui-unhide applied)
If you have applied `tch-gui-unhide`, you can configure `opkg` and install/remove packages through the user interface.

## de-telstra
If you are are a [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) user, simply run it with `-o` option to correctly configure `opkg` on your device.

## Manual Configuration
Edit the `/etc/opkg/distfeeds.conf` file and insert a `#` before the repository listed, otherwise you will get errors when updating, because that repository does not exist.

Edit the following files to configure `opkg`:
#### Firmware Versions starting with 17
- /etc/opkg.conf
```
dest root /
dest ram /tmp
lists_dir ext /var/opkg-lists
option overlay_root /overlay
arch all 1
arch noarch 1
arch brcm63xx-tch 30
```
- /etc/opkg/customfeeds.conf
```
src/gz base https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/base
src/gz luci https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/luci
src/gz management https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/management
src/gz packages https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/packages
src/gz routing https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/routing
src/gz telephony https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/telephony
```

#### Firmware Versions starting with 18 and later
- /etc/opkg.conf
```
dest root /
dest ram /tmp
lists_dir ext /var/opkg-lists
option overlay_root /overlay
arch all 1
arch noarch 1
arch brcm63xx-tch 30
arch arm_cortex-a9 10
arch arm_cortex-a9_neon 20
```
- /etc/opkg/customfeeds.conf
```
src/gz chaos_calmer_base_macoers https://www.macoers.com/repository/homeware/18/brcm63xx-tch/VANTW/base
src/gz chaos_calmer_packages_macoers https://www.macoers.com/repository/homeware/18/brcm63xx-tch/VANTW/packages
src/gz chaos_calmer_luci_macoers https://www.macoers.com/repository/homeware/18/brcm63xx-tch/VANTW/luci
src/gz chaos_calmer_routing_macoers https://www.macoers.com/repository/homeware/18/brcm63xx-tch/VANTW/routing
src/gz chaos_calmer_telephony_macoers https://www.macoers.com/repository/homeware/18/brcm63xx-tch/VANTW/telephony
src/gz chaos_calmer_core_macoers https://www.macoers.com/repository/homeware/18/brcm63xx-tch/VANTW/target/packages
```

## Post Configuration
After configuring opkg, you need to update the package lists. You need to do this each time before you install any packages:
```
opkg update
```

You should also update the system CA certificates. You can do this either by:
* preferably downloading and running the [`update-ca-certificates`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#update-ca-certificates) script; **or**
* manually installing the CA certificates packages using: `opkg install ca-certificates ca-bundle`

The `update-ca-certificates` script will install the latest available certificates (and gives you the option to schedule a regular job to update them), whereas the opkg packages may not contain the latest certificates.