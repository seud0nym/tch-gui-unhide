# Unlock the GUI on your Telstra Technicolor Device
These scripts can be applied to various Telstra branded Technicolor devices to unlock hidden features in the web GUI, and automate many of the functions required to set up your device correctly.

## TL;DR
Skip any of these steps that you have already done.
1. Root your device (see https://hack-technicolor.rtfd.io) and ensure it is running a supported firmware version.
2. [Download](https://github.com/seud0nym/tch-gui-unhide/releases/latest) the latest release for your firmware.
3. Download any [extra feature scripts](https://github.com/seud0nym/tch-gui-unhide/tree/master/extras) for opkg packages you have installed. 
4. Copy the downloaded file(s) into the /root or /tmp directory of your device, or onto your USB stick (I normally use a USB stick so that the scripts are not lost if the device is reset, otherwise I use /root so the scripts are in the root user home directory).
5. Change to the directory containing the release, aand extract the files using the command: `tar -xzvf <filename>`
6. Set the optimal bank plan. Run `./show-bank-plan` to see if your bank plan is optimal,and if not, execute: `./set-optimal-bank-plan` (*WARNING: This will reboot your device*)
7. Harden root access and disable un-needed services with the [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) script. Run `./de-telstra -?` to see available options, or for some sensible settings, just execute: `./de-telstra -A`
8. Change the root password by executing: `passwd`
9. If you are running FW 17.2.0468 or later, optionally create your *ipv4-DNS-Servers* and/or *ipv6-DNS-Servers* files in the same directory as the scripts. (See [**Firmware Versions 0468 and Later**](https://github.com/seud0nym/tch-gui-unhide#firmware-versions-0468-and-later))
10. Apply the GUI changes. Run `./tch-gui-unhide -?` to see available options, or just execute: `./tch-gui-unhide`
11. Clear cached images and files from your browser to see the changes.

#### NOTES:
- If you reset your device, *or* restore it to a state before you applied the scripts, *or* upgrade the firmware, you will need to run `de-telstra` and `tch-gui-unhide` again!
- To change the GUI theme, run `./tch-gui-unhide -T` with your theme options, and only the theme will be applied without re-applying all the other changes that the script usually makes.
- You can revert to the Telstra GUI with the command: `./tch-gui-unhide -r`

Read on for the long version...

*PLEASE NOTE: Previous versions of the `tch-gui-unhide` script (release 2020.08.03 and before) also applied most of the hardening recommendations from https://hack-technicolor.readthedocs.io/en/stable/Hardening/. These have now been moved to a separate utility script ([`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra)).*

## Why not just use https://github.com/Ansuel/tch-nginx-gui?
When I first acquired root access to my DJA0231, I applied tch-nginx-gui because I had used it for some time on my Technicolor TG800vac, and found it very good. However, on the DJA0231, I encountered various problems after reboots, such as loss of customised SSID's and IP addresses, loss of root in one instance, the admin password got reset to Telstra default, and so on.

So, I set out to enable whatever hidden features were included with the firmware by default, without making significant changes to the GUI code so as to maintain stability, and to try and make it almost as pretty as the tch-nginx-gui. Since then, it has been expanded to incorporate some features of the Ansuel GUI, but the original goal of stability is unchanged. No features are enabled if stability is compromised. No system executables (other than GUI code) are added or replaced. The aim of this script is to make as few changes as possible to be as stable as possible, but also to unlock as many features as is practicable.

## What GUI features are unlocked?
- Configuration export/import, and the export file name is changed from *config.bin* to *VARIANT-SERIAL-VERSION@yymmdd.bin* (i.e. it includes the hardware type, serial number, firmware version and the current date)
- Firmware update from the GUI
- The SMS tab is enabled on the **Mobile** screen
- You can disable/enable the radios from the **Wi-Fi** card (without opening the screen), and the status of Guest SSIDs is displayed
- You can edit the host names on the **Device** screen
- The **Telephony** screen has the following tabs enabled:
    - Global (Enable/Disable telephony + SIP network provider details)
    - Phone Book (Set up contacts)
    - Phone Numbers (Define SIP profiles)
    - Service (Configure VoIP services)
    - Incoming/Outgoing Map (Assign phone numbers to handsets)
    - Call Log
- On the **Diagnostics** screen, the following extra tabs are enabled:
    - Network Connections
    - TCP Dump
- The **IP Extras** card is enabled to:
    - show IPv4 Routes
    - allow configuration of IPv4 static routes
    - show DNS servers
- The **CWMP** card is enabled to show the status of and configure CWMP (only if CWMP is enabled)
- The **System Extras** card is enabled to:
    - Allow you to trigger a BOOTP upgrade (**BEWARE! Do not press this button unless you know what you are doing!**)
    - Turn SSH access on or off for the WAN/LAN
    - Configure an external syslog server
- The **NAT Helpers** card is enabled where you can enable or disable ALG's for FTP, IRC, SIP, PPTP, RTSP, SNMP, TFTP and AMANDA
- The **xDSL Config** card is enabled where you can see xDSL core settings

Some hidden screens included on the device are not enabled, mainly because they fail and cause issues in the GUI, or sometimes more work would be required to implement them than can be done with simple stream editing of the existing files. Some of them are only applicable to older versions of the firmware.

### GUI Features incorporated from the Ansuel Custom GUI
- **Broadband** screen allows:
    - Disable/enable WAN Sensing
    - Set connection mode manually when WAN Sensing disabled
    - Allow VLAN ID tagging
- **Internet** allows:
    - Set connection parameters
    - Specify DNS servers used by the device
- Additional tabs and features on the **IP Extras** card/screen:
    - Customise DNS by network interface
    - Policy routing for mwan
    - Bridge grouping
    - DoS protection
- Modified **System Extras** card to show:
    - SSH LAN status
    - SSH WAN status
- Modified **QoS** card
- Modified **Telephony** Global tab to allow:
    - Add/delete SIP providers (up to maximum of 2)
    - Editing of the name, and setting of interface (LAN/WAN/etc.)
- Added the **Telephony** Codecs tab
- Greater control over the **Wi-Fi** output power

### Additional (new) GUI Features
- **WiFi Boosters** card (only for devices with multiap installed - i.e. DJA0230 and DJA0231)
- **Traffic monitor** tab in Diagnostics

## What else does it do?
- Properly enables SSH LAN access (because if you don't and then disable it through the GUI, you can lose SSH access).
- Modernises the GUI a little bit with a light theme by default, or optionally with a dark (night) theme. See Themes below.

*PLEASE NOTE: Previous versions of the script (release 2020.08.16 and before) also set the hostname and domain. This functionality has been moved to the [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) utility script.*

### Firmware Versions 0468 and Later
- If a file called *ipv4-DNS-Servers* and/or *ipv6-DNS-Servers* is found in the directory from which the script is invoked, the contents will be added to the list of DNS Servers on the **Local Network** screen.

    The contents of these file are simply the hostname and IP address, which are separated by a space. Multiple servers may be added, each on its own line.

    For example, my *ipv4-DNS-Servers* and *ipv6-DNS-Servers* files consists of my main and backup [Pi-hole](https://pi-hole.net/) servers:
```
    Pi-hole 192.168.0.168
    Pi-hole-VM 192.168.0.192
```

```
    Pi-hole fe80::aaaa:bbbb:cccc:dddd
    Pi-hole-VM fe80::1:22:3300:444
```

### Firmware Version 18.1.c.0514 Specific
- If run this script on the DJA0231 with the 18.1.c.0514 firmware, it will also add a button to access the pre-release version of DumaOS, but only if DumaOS has been enabled. To add this button, execute the [`dumaos-beta`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#dumaos-beta) script with the `-on` parameter to enable DumaOS before running the `tch-gui-unhide` script.

*PLEASE NOTE: Previous versions of the script (release 2020.08.03 and before) also automatically enabled the beta version of DumaOS on the DJA0231. To get this functionality, run the [`dumaos-beta`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#dumaos-beta) utility script.*

## What does it actually do?
All configuration changes are applied using the *uci* command interface to update the configuration files.

GUI changes are implemented by making edits on the existing files.

No new executables are added to the system outside of the GUI changes.

## How do I run it?
Basically, the steps are:

### First, you need to get root access to your device and ensure it is running the correct firmware:
Follow the instructions at https://hack-technicolor.rtfd.io

The scripts have only be tested against the specified firmware version denoted in the script name. If you are not running the firmware version for which the script was written, you should upgrade. (https://hack-technicolor.readthedocs.io/en/latest/Repository)

Click [`here`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities) for information about the utility scripts.

### Second, download and extract the scripts
Download and extract the release archive directly to the device with the following command:
```
 curl -k -L https://github.com/seud0nym/tch-gui-unhide/releases/latest/download/$(uci get version.@version[0].version | cut -d- -f1).tar.gz | tar -xzvf -
```
The above command will only work if run from a supported firmware version, with internet access.

Alternatively, you can download the release for your firmware version to your computer and then upload it up to your device using WinSCP or equivalent. Run the `tar -xzvf <filename>` command to extract the release files.

The best location for the scripts on your device is on a USB stick, so that if you need to reset or re-apply the firmware, the scripts will still be available without needing to uplpad them to the device again. Otherwise, I normally put them in the /root directory (the root user home directory) so they are available as soon as you log in without changing to another directory. /tmp is also suitable.

#### Harden your root access
It is recommended that you apply whatever hardening (such as the [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) script) and other configuration changes you want to make *before* executing the script, as some features are enabled or disabled depending on the current configuration of the target device.

### Third, optionally create customisation files (FW versions 0468 and later only)
- Create your *ipv4-DNS-Servers* and/or *ipv6-DNS-Servers* files in the same directory as the scripts, as specified above.

### Last, execute the script
**PLEASE NOTE:** The GUI files will be restored to their original state (from /rom/www/...) by the script before it makes its modifications. If you have previously installed the Ansuel tch-nginx-gui, you should remove it before running this script.
```
./tch-gui-unhide <options>
```
The script accepts the following options:
- -t l|n|t
    - Set a light (*-tl*), night (*-tn*), or default Tesltra (*-tt*) theme
    - The default is the current setting, or Telstra if no theme has been applied.
- -c b|o|g|p|m
    - Set the theme highlight colour: *-cb*=blue *-co*=orange *-cg*=green *-cp*=purple *-cm*=monochrome
    - The default is the current setting, or *-cm* for the light theme or *-cb* for the night theme.
- -i y|n
    - Show (*y*) or hide (*n*) the card icons.
    - The default is the current setting, or *-in* for the light theme and *-iy* for the night theme.
- -l
    - Keep the Telstra landing page
- -T
    - Apply the theme settings *ONLY*. All other processing is bypassed.
- -x *c*|*m*|*n*|*p*|*q*|*r*|*s*|*x*|*A*
    - Exclude specific cards: *-xc*=Content Sharing *-xm*=Management *-xn*=NAT Helpers *-xp*=Printer Sharing *-xq*=QoS *-xr*=Relay Setup *-xs*=System Extras *-xx*=xDSL config *-xA*=ALL
    - Use the *-x* option multiple times to specify multiple cards, or use *-xA* for all of the above cards.
- -n *c*|*m*|*n*|*p*|*q*|*r*|*s*|*x*|*A*
    - Include specific cards that were previously excluded.
    - Use the *-n* option multiple times to specify multiple cards, or use *-nA* for all cards.
- -y
    - Allows you to skip the initial prompt to confirm execution, and automatically responds with **y**.
- -r
    - Allows you to revert the *GUI* changes. Configuration changes are **NOT** undone!
- -u
    - Check for and download any updates to the firmware-specific version of `tch-gui-unhide`
- -U
    - Download the latest release, including utility scripts (will overwrite all existing script versions)
- -?
    - Displays usage information

NOTE: The theme (-t) and excluded/included cards (-x/-n) options do not need to be re-specified when re-running the script: state will be 'remembered' between executions (unless you execute with the -r option, which will remove all state information).

The `tch-gui-unhide` script is a short-cut to the actual script for your firmware, which is named `tch-gui-unhide-<version>` (e.g. `tch-gui-unhide-18.1.c.0462`). If you get a "Platform script not found" error running this script, download the correct release for your firmware configuration.

The firmware version will be checked during execution. If it does not match the target version, you will be prompted to exit or force execution. This is **YOUR** decision to proceeed.

The script will restart and reload services for which it has modified configuration. Subsequent executions will not re-apply configuration already set correctly, and therefore will not restart services unnecessarily.

### Finally, clear your browser cache
To see the updated logo and icons and to correctly apply the updated style sheet, you will probably need to clear cached images and files from your browser.

## Other Important Things To Note
- The script changes will not persist a reset or restore. If you factory reset your device, or restore to it a state before you applied the script, or upgrade/install firmware, you will need to run the script again!

# Themes
*PLEASE NOTE: Previous releases of this script had additional scripts to switch between the light and dark themes. This release includes multiple themes in the single script.*

By default, the script will apply a "light" theme, similar to the DumaOS theme.

You can switch to a "dark" (or "night") theme by re-running the script with the `-T`, `-t`, `-c` and `-i` parameters

- The `-T` parameter bypasses all processing except the application of the theme. Without this option, the entire script will be re-applied with the selected theme.
- The `-t` parameter has 3 options: `-tl` for the light theme, or `-tn` for the night theme, or `-tt` for the Telstra theme.
- The `-c` parameter specifies a highlight colour for the selected theme. There are 5 choices: blue (`-cb`), orange (`-co`), green (`-cg`), purple (`-cp`) or monochrome (`-cm`). If no theme has been previously applied, the default colour for the light theme is monochrome, and blue for the night theme.
- The `-i` parameter controls whether or not the background icons on the cards will be displayed. `-iy` makes the icons visible; `-in` hides them. If no theme has been previously applied, the default is no icons for the light theme, and icons visible for the night theme.
- The `-l` parameter turns of theming and de-branding of the Telstra landing page (DJA0231).

When you re-run the script without specifying a theme, it will default to the previously select theme, or the "Telstra" theme when run the first time or immediately after running the script with the `-r` option to revert all GUI changes.

After applying a theme, you will need to clear cached images and files from your browser to see the changes.

# Thanks
This would not have been possible without the following resources:

- https://hack-technicolor.rtfd.io
- https://forums.whirlpool.net.au/thread/9vxxl849
- https://github.com/Ansuel/tch-nginx-gui

Thank you to you all.
