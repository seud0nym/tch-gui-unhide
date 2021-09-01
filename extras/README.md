# Extras

This is where extra functionality scripts can be found. These are not incorporated in the main tch-gui-unhide code line, because they rely on additional packages being installed.

Extras scripts that rely on packages to be installed require `opkg` to be configured correctly on your device. See **opkg Configuration** [`below`](https://github.com/seud0nym/tch-gui-unhide/tree/master/extras#opkg-Configuration).

## tch-gui-unhide-xtra.adblock
Creates a GUI interface for the [`Adblock`](https://openwrt.org/packages/pkgdata/adblock) package that allows you to block ads at the router level.
#### Download
https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/extras/tch-gui-unhide-xtra.adblock
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
#### Post-Installation Configuration
- Configure the IPv4 Primary DNS Server under Local Network to the local device (depending on your version of tch-gui-unhide, this will be "RSP/ISP DNS Servers", "Custom WAN DNS Servers", or the device variant (e.g. "DJA0230" or "DJA0231"))
- Leave the IPV4 Secondary DNS Server as blank
- Configure your preferred DNS Servers under Internet Access
- Enable DNS Intercept under Firewall (leave DNS Server Address blank)
#### Changes External to the GUI
The installation creates the the following transformer UCI mappings and commit/apply scripts to support the GUI changes:
- /usr/share/transformer/commitapply/uci_adblock.ca
- /usr/share/transformer/mappings/rpc/gui.adblock.map
- /usr/share/transformer/mappings/uci/adblock.map
#### Removal Instructions
1. Delete `tch-gui-unhide-xtra.adblock`
2. Remove adblock and dependencies: `opkg remove adblock uclient-fetch`
    - If you are on firmware 18.1.c, then also: `opkg remove libustream-openssl`
3. Re-run `tch-gui-unhide` to remove the GUI changes, and the additional transformer mappings

## tch-gui-unhide-xtra.minidlna
Replaces the stock DLNA server management in the GUI so that it supports OpenWRT minidlna.
#### Firmware Applicability
Should be applicable to all firmware versions supported by `tch-gui-unhide`.
#### Download
https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/extras/tch-gui-unhide-xtra.minidlna
#### Prerequisites 
Install minidlna using the opkg command: `opkg install minidlna`
#### Changes External to the GUI
The installation creates the the following transformer UCI mappings and commit/apply scripts to support the GUI changes:
- /usr/share/transformer/commitapply/uci_minidlna.ca
- /usr/share/transformer/mappings/uci/minidlna.map
#### Removal Instructions
1. Do **not** delete `tch-gui-unhide-xtra.minidlna`
2. Remove minidlna: `opkg remove minidlna`
3. Re-run `tch-gui-unhide` to remove the GUI changes, custom configuration and the additional transformer mappings
4. Now you can delete `tch-gui-unhide-xtra.minidlna`

## tch-gui-unhide-xtra.rsyncd
Adds the ability to enable and disable the rsync daemon from the GUI.
#### Firmware Applicability
Should be applicable to all firmware versions supported by `tch-gui-unhide`.
#### Download
https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/extras/tch-gui-unhide-xtra.rsyncd
#### Prerequisites 
Install rsyncd using the opkg command: `opkg install rsync rsyncd`
#### Changes External to the GUI
- Adds /usr/share/transformer/mappings/rpc/gui.rsync.map to allow enabling/disabling of daemon via the GUI
- Adds the *home* module to /etc/rsyncd.conf to allow read/write access to the /root directory via rsync (e.g. `rsync 192.168.0.1::home`)
- Adds the *tmp* module to /etc/rsyncd.conf to allow read/write access to the /tmp directory via rsync (e.g. `rsync 192.168.0.1::tmp`)
- Adds the *usb* module to /etc/rsyncd.conf to allow read/write access to the USB device via rsync (e.g. `rsync 192.168.0.1::usb`)
#### Removal Instructions
1. Delete `tch-gui-unhide-xtra.rsyncd`
2. Remove rsyncd: `opkg remove rsync rsyncd`
3. Re-run `tch-gui-unhide` to remove the GUI changes, and the additional transformer mappings

## tch-gui-unhide-xtra.samba36-server
Correctly configures OpenWRT SAMBA 3.6 to provide SMBv2 for Windows 10 inter-operability. This update adds the ability to change the password via the GUI.
#### Firmware Applicability
You should only install SAMBA 3.6 on the 17.2 and 18.1.c firmware. The 20.3.c firmware contains NQE rather than SAMBA, and does not require the SAMBA 3.6 update to upgrade to SMBv2 and inter-operate with Windows 10.
#### Download
https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/extras/tch-gui-unhide-xtra.samba36-server
#### Prerequisites
Install SAMBA v3.6 using the opkg command: `opkg --force-overwrite install samba36-server`
#### Changes External to the GUI
- Configures SAMBA to use SMBv2 by default
- Creates the samba user and group, with NO password (change the password in the GUII or with the `smbpasswd` command)
- Adds the ability to set share users via UCI and transformer
- Adds /usr/share/transformer/mappings/rpc/gui.samba.map to allow setting of password via the GUI
- Configures the default USB share to require the samba user
#### Removal Instructions
1. Do **not** delete tch-gui-unhide-xtra.samba36-server
2. Remove samba36-server: `opkg remove samba36-server`
3. Re-run `tch-gui-unhide` to correctly restore the default version of SAMBA on the device and remove the GUI changes, the custom configuration and the additional transformer mappings
4. Now you can delete `tch-gui-unhide-xtra.samba36-server`

# How to download and execute these scripts
Download the scripts that you wish to execute into the same directory as `tch-gui-unhide`.

**NOTE: Replace `<scriptname>` with the name of the script you wish to download.**

Execute these commands on your device via a PuTTY session or equivalent (an active WAN/Internet connection is required):
```
wget https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/extras/<scriptname> 
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