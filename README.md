# Unlock the GUI on your Telstra Technicolor Device
These scripts can be applied to various Telstra branded Technicolor devices to unlock hidden features in the web GUI, and automate many of the functions required to set up your device correctly.

## TL;DR
Skip any of these steps that you have already done.
1. Root your device (see https://hack-technicolor.rtfd.io) and ensure it is running a supported firmware version.
2. [Download](https://github.com/seud0nym/tch-gui-unhide/releases/latest) the latest release for your firmware.
3. Copy the downloaded file(s) into the /root or /tmp directory of your device, or onto your USB stick (I normally use a USB stick so that the scripts are not lost if the device is reset, otherwise I use /root so the scripts are in the root user home directory).
4. Change to the directory containing the release, and extract the files using the command: `tar -xzvf <filename>`
5. Set the optimal bank plan. Run `./show-bank-plan` to see if your bank plan is optimal,and if not, execute: `./set-optimal-bank-plan` (*WARNING: This will reboot your device*)
6. Harden root access and disable un-needed services with the [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) script. Run `./de-telstra -?` to see available options, or for some sensible settings, just execute: `./de-telstra -A`
7. Change the root password by executing: `passwd`
8. Optionally, download any [extra feature scripts](https://github.com/seud0nym/tch-gui-unhide/tree/master/extras) you want to install into the same directory as the scripts. 
9. Optionally create your *ipv4-DNS-Servers* and/or *ipv6-DNS-Servers* files in the same directory as the scripts. (See [**Custom DNS Servers**](https://github.com/seud0nym/tch-gui-unhide#custom-dns-servers))
10. Apply the GUI changes. Run `./tch-gui-unhide -?` to see available options, or just execute: `./tch-gui-unhide`
11. Optionally run `./tch-gui-unhide-cards` to change card sequence and visibility
12. Clear cached images and files from your browser to see the changes.

#### NOTES:
- If you reset your device, *or* restore it to a state before you applied the scripts, *or* upgrade the firmware, you will need to run `de-telstra` and `tch-gui-unhide` again!
- To change the GUI theme, run `./tch-gui-unhide -T` with your theme options, and only the theme will be applied without re-applying all the other changes that the script usually makes. Alternatively, you can change the theme from within the GUI (under **Management**).
- You can revert to the Telstra GUI with the command: `./tch-gui-unhide -r`

Read on for the long version...

## Why not just use https://github.com/Ansuel/tch-nginx-gui?
When I first acquired root access to my DJA0231, I applied tch-nginx-gui because I had used it for some time on my Technicolor TG800vac, and found it very good. However, on the DJA0231, I encountered various problems after reboots, such as loss of customised SSID's and IP addresses, loss of root in one instance, the admin password got reset to Telstra default, and so on.

So, I set out to enable whatever hidden features were included with the firmware by default, without making significant changes to the GUI code so as to maintain stability, and to try and make it almost as pretty as the tch-nginx-gui. Since then, it has been expanded to incorporate some features of the Ansuel GUI, but the original goal of stability is unchanged. No features are enabled if stability is compromised. No system executables (other than GUI code) are added or replaced. The aim of this script is to make as few changes as possible to be as stable as possible, but also to unlock as many features as is practicable.

## What GUI features are unlocked?
- Configuration export/import, and the export file name is changed from *config.bin* to *VARIANT-SERIAL-VERSION@yymmdd.bin* (i.e. it includes the hardware type, serial number, firmware version and the current date)
- Optionally enables firmware update through the GUI (**BEWARE! This functionality also does a reset to factory defaults, and you will lose root!**)
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
    - Can now switch active bank from the screen
- Modified **QoS** card
- Modified **Telephony** Global tab to allow:
    - Add/delete SIP providers (up to maximum of 2)
    - Editing of the name, and setting of interface (LAN/WAN/etc.)
- Added the **Telephony** Codecs tab
- Greater control over the **Wi-Fi** output power
- Modified **Time of Day** Wireless Control tab to allow selection of access point
- **Management** of Init scripts and cron tasks schedules

### Additional (new) GUI Features
- **Gateway** card now has current device status for CPU usage, free RAM and temperature
- **Broadband** card shows current upload/download volume, and average per day
- **Internet Access** and **LAN** cards now show IPv6 information
- **Internet Access** screen allows you to specify the WAN Supervision mode, both IPv4 and IPv6 custom DNS Servers, and to set a static IPv6 address (in static connection mode)
- **Local Network** allows enabling/disabling of DHCPv6 and SLAAC
- **WiFi** card auto-updates to reflect SSID status (e.g. when Time of Day Wireless Access Controls rules enable or disable SSIDs)
- **WiFi Boosters** card (only for devices with multiap installed - i.e. DJA0230 and DJA0231)
- **Devices** card auto-refreshes, and also shows separately  the count of WiFi devices connected via a WiFi Booster (only for devices with multiap installed - i.e. DJA0230 and DJA0231)
- **Traffic monitor** tab in Diagnostics
- **Time of Day** card shows the Wireless Control rule count, and correctly applies changes so that they work reliably
- **System Extras** now allows:
    - Configure WAN SSH access
    - Change the web GUI theme
- **Management** screen allows the theme to be changed from within the GUI, and viewing of running processes
- **Firewall** cards shows whether IPv4 and IPv6 pings are allowed, and the screen allows you to specify src and/or dest zone for user defined rules, and therefore create incoming, outgoing and forwarding rules in either direction (stock GUI only creates lan->wan forwarding rules)
- **Telephony** card shows call statistics (number of calls in, missed and out)
- **Telephony** screen now has a Dial Plans tab to edit the dial plans, and you can optionally show the decrypted SIP passwords on the Profiles tab
- **Mobile** screen now has a Network Operators tab to modify the allowed Mobile Country Code (MCC) and Mobile Network Code (MNC) combinations

## What else does it do?
- Properly enables SSH LAN access (because if you don't and then disable it through the GUI, you can lose SSH access).
- Modernises the GUI a little bit with a choice of light, dark (night) or Telstra (Classic or Modern) theme. See Themes below.
- Optionally enables or disables default user access (i.e. no login required to access the Web interface).
- Allows you to change the sequence of the cards and their visibility. See Cards below.
- Optionally uses a decrypted text field (instead of masked password field) for SIP Profile passwords.

### Custom DNS Servers
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

### Firmware Versions 0468 and Later
*PLEASE NOTE: Since release 2020.11.20, Custom DNS Servers (above) can now be specified for all supported releases, including FW 17.2.0284.*

### Firmware Version 18.1.c.0514 (and later) Specific
- If you run this script on the 18.1.c.0514 or later firmware, it can also add a button to access DumaOS (Telstra Game Optimiser), but only if DumaOS has been enabled. To add this button, execute the [`dumaos`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#dumaos) script with the `-on` parameter to enable DumaOS *before* running the `tch-gui-unhide` script.

## What does it actually do?
All configuration changes are applied using the *uci* command interface to update the configuration files.

GUI changes are implemented by making edits on the existing files, or adding new/replacement files.

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

The best location for the scripts on your device is on a USB stick, so that if you need to reset or re-apply the firmware, the scripts will still be available without needing to upload them to the device again. Otherwise, I normally put them in the /root directory (the root user home directory) so they are available as soon as you log in without changing to another directory. /tmp is also suitable.

#### Harden your root access
It is recommended that you apply whatever hardening (such as the [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) script) and other configuration changes you want to make *before* executing the script, as some features are enabled or disabled depending on the current configuration of the target device.

### Third, optionally create customisation files
- Create your *ipv4-DNS-Servers* and/or *ipv6-DNS-Servers* files in the same directory as the scripts, as specified above.

### Last, execute the script
**PLEASE NOTE:** The GUI files will be restored to their original state (from /rom/www/...) by the script before it makes its modifications. If you have previously installed the Ansuel tch-nginx-gui, you should remove it before running this script.
```
./tch-gui-unhide <options>
```
The script accepts the following options:
- -t l|n|t|m
    - Set a light (*-tl*), night (*-tn*), or default Telstra Classic (*-tt*) or Telstra Modern (*-tm*) theme
    - The default is the current setting, or Telstra Classic if no theme has been applied
- -c b|o|g|p|r|m
    - Set the theme highlight colour: *-cb*=blue *-co*=orange *-cg*=green *-cp*=purple *-cr*=red *-cm*=monochrome
    - The default is the current setting, or *-cm* for the light theme or *-cb* for the night theme
- -i y|n
    - Show (*y*) or hide (*n*) the card icons.
    - The default is the current setting, or *-in* for the light theme and *-iy* for the night theme
- -h d|s|n|"text"
    - Set the browser tabs title to VARIANT (d), VARIANT-MAC_ADDRESS (s), HOSTNAME (n) or ("text") the specified "text".
    - Default is the current setting
- -d y|n
    - Enable (y) or Disable (n) the default user (i.e. no login required to access the Web interface)
    - The default is the current setting
- -f y|n
    - Enable (y) or Disable (n) Firmware upgrade in the web GUI
    - The default is the current setting
- -l y|n
    - Keep the Telstra landing page (y) or de-brand the landing page (n)
    - The default is current setting, or (n) if no theme has been applied
- -p y|n
    - Use decrypted text field (y) or masked password field (n) for SIP Profile passwords
    - The default is current setting (i.e. (n) by default)
- -T
    - Apply the theme settings *ONLY*. All other processing is bypassed.
- -y
    - Allows you to skip the initial prompt to confirm execution, and automatically responds with **y**
- -r
    - Allows you to revert the *GUI* changes. Configuration changes are **NOT** undone!
- -u
    - Check for and download any updates to the firmware-specific version of `tch-gui-unhide`
- -U
    - Download the latest release, including utility scripts (will overwrite all existing script versions)
- -?
    - Displays usage information

NOTE: The theme options (-t, -c and -i) do not need to be re-specified when re-running the script: state will be 'remembered' between executions (unless you execute with the -r option, which will remove all state information).

The `tch-gui-unhide` script is a short-cut to the actual script for your firmware, which is named `tch-gui-unhide-<version>` (e.g. `tch-gui-unhide-18.1.c.0462`). If you get a "Platform script not found" error running this script, download the correct release for your firmware configuration.

The firmware version will be checked during execution. If it does not match the target version, you will be prompted to exit or force execution. This is **YOUR** decision to proceed.

The script will restart and reload services for which it has modified configuration. Subsequent executions will not re-apply configuration already set correctly, and therefore will not restart services unnecessarily.

### Finally, clear your browser cache
To see the updated logo and icons and to correctly apply the updated style sheet, you will probably need to clear cached images and files from your browser.

## Other Important Things To Note
- The script changes will not persist a reset or restore. If you factory reset your device, or restore to it a state before you applied the script, or upgrade/install firmware, you will need to run the script again!

# Cards
The `tch-gui-unhide-cards` is an interactive script that allows you to re-order and change the visibility of the cards. When you execute it, it will display the current card configuration. Follow the on-screen prompts to re-order and hide/show cards.

**NOTE:** You must execute `tch-gui-unhide` *BEFORE* running `tch-gui-unhide-cards`!

The screen will look something like this:
```
tch-gui-unhide-cards

 1 DJA0231            2 Broadband          3 Internet Access    4 Mobile
 5 Wi-Fi              6 Wi-Fi Boosters     7 Local Network      8 Devices
 9 WAN Services      10 Telephony         11 Firewall          12 QoS
13 Diagnostics       14 Management        15 Content Sharing   16 Printer Sharing
17 Parental Controls 18 IP Routing        19 Time of Day       20 CWMP
21 System Extras     22 NAT Helpers       23 xDSL Config       24 (Relay Setup)

NOTE: Titles in () indicate hidden cards. They will always appear above after the last visible card.
NOTE: 'CWMP' card is only visible if CWMP has not been disabled
NOTE: 'xDSL Config' card is only visible on DSL connections

OPTIONS: A single card number between 1 and 24
         d = order cards by the default sequence
         m = hide all optional feature cards
         s = order cards by a suggested sequence
         u = undo changes
         a = apply changes and quit
         q = quit without applying changes

Enter one of 1-24/d/m/s/u/a/q:
```

This screen displays the cards in the currently configured sequence. Any hidden cards are shown after all visible cards, with their titles in brackets.

When this screen is displayed, you have the following options available:
- *Card number*
    - Enter a card number (between 1 and 24) to access a sub-menu that offers the following options:
        - h
            - Hide the selected card
        - u
            - Unhide the selected card (make it visible)
        - Card number
            - Move the card to the specified position
    - e.g. 
        - To move the *Diagnostics* card to the last position, you would enter *13*, and then *23*. 
        - To hide the *Printer Sharing* card, you would enter *16* and then *h*.
    - To exit from the sub-menu without making a change, just press Enter.
- *d*
    - Re-orders the cards into the stock sequence. All cards are also made visible.
- *m*
    - A minimal card list that hides all optional feature cards.
- *s*
    - Re-orders the cards into a suggested sequence, and hides some rarely used cards.
- *u*
    - Reverts any changes that you have not yet applied during this execution.
- *a*
    - Applies the changes you have made.
- *q*
    - Quits the scripts without making any changes.

## Other Important Things To Note
- Script changes will not persist a reset or restore. If you factory reset your device, or restore to it a state before you applied a script, or upgrade/install firmware, you will need to run the script again!

# Themes
By default, the script will keep a "Telstra" theme, very similar to the default Telstra theme.

You can switch to a "dark" (or "night") theme by re-running the script with the `-T`, `-t`, `-c` and `-i` parameters:

- The `-T` parameter bypasses all processing except the application of the theme. Without this option, the entire script will be re-applied with the selected theme.
- The `-t` parameter has 3 options: `-tl` for the light theme, or `-tn` for the night theme, `-tt` for the Telstra Classic theme, or `-tm` for the Telstra Modern theme.
- The `-c` parameter specifies a highlight colour for the selected theme. There are 5 choices: blue (`-cb`), orange (`-co`), green (`-cg`), purple (`-cp`) or monochrome (`-cm`). If no theme has been previously applied, the default colour for the light theme is monochrome, and blue for the night theme.
- The `-i` parameter controls whether or not the background icons on the cards will be displayed. `-iy` makes the icons visible; `-in` hides them. If no theme has been previously applied, the default is no icons for the light theme, and icons visible for the night theme.
- The `-l` parameter turns off theming and de-branding of the Telstra landing page (DJA0231).

When you re-run the script without specifying a theme, it will default to the previously select theme, or the "Telstra" theme when run the first time or immediately after running the script with the `-r` option to revert all GUI changes.

You can also change the theme, colour variation and icon visibility from within the GUI (in the *Management* screen).

# Thanks
This would not have been possible without the following resources:

- https://hack-technicolor.rtfd.io
- https://forums.whirlpool.net.au/thread/9vxxl849
- https://github.com/Ansuel/tch-nginx-gui

Thank you to you all.
