# Unlock the GUI on your Telstra Technicolor Modem
These scripts can be applied to the Telstra Gateway MAX 2 (Technicolor TG800vac) and the Telstra Smart Modem Gen 2 (Technicolor DJA0231) to unlock hidden features in the web GUI.

*PLEASE NOTE: Previous versions of the `tch-gui-unhide` script (release 2020.08.03 and before) also applied most of the hardening recommendations from https://hack-technicolor.readthedocs.io/en/stable/Hardening/. These have now been moved to a separate utility script ([`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities)).*

## Why not just use https://github.com/Ansuel/tch-nginx-gui?
When I first rooted my DJA0231, I applied the tch-nginx-gui because I had used it for some time on my Technicolor TG800vac, and found it very good.

However, on the DJA0231, I encountered various problems after reboots, such as loss of customised SSID's and IP addresses, loss of root in one instance, the admin password got reset to Telstra default, and so on.

So, I set out to enable whatever hidden features were included with the firmware by default, without making significant changes to the GUI code, and to try and make it almost as pretty as the tch-nginx-gui.

The script does not expose features not present in the stock GUI or the hidden cards, and therefore is not as comprehensive as the Ansuel GUI. It is a trade-off: stability vs features. The aim of this script is to make as few changes as possible to be as stable as possible.

## What GUI features are unlocked?
- Configuration export/import, and the export file name is changed from *config.bin* to *VARIANT-VERSION-yymmdd.bin* (i.e. it includes the hardware type, firmware version and the current date)
- Firmware update from the GUI
- The SMS tab is enabled on the **Mobile** screen
- The **Telephony** screen has the following tabs enabled:
    - Global (Enable/Disable telephony + SIP network provider details)
    - Phone Book (Set up contacts)
    - Phone Numbers (Define SIP profiles)
    - Service (Configure VoIP services)
    - SIP Device (Shows SIP mappings)
    - Incoming/Outgoing Map (Assign phone numbers to handsets)
    - Statistics
    - Call Log
- The TCP Dump tab is enabled on the **Diagnostics** screen
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

Some screens included on the device are not enabled, mainly because they fail and cause issues in the GUI, or sometimes more work would be required to implement them than can be done with simple stream editing of the existing files.

*PLEASE NOTE: Previous versions of the script (release 2020.08.03 and before) also automatically enabled the beta version of DumaOS on the DJA0231. To get this functionality, run the [`dumaos-beta`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities) utility script (see below).*

## What else does it do?
- You can set the hostname of the device and the domain of the LAN. Default Telstra hostnames are removed.
- Properly enables SSH LAN access (because if you don't and then disable it through the GUI, you can lose SSH access)
- Modernises the GUI a little bit with a light theme by default, or optionally with a dark (night) theme

### DJA0231 Specific
- If a file called *ipv4-DNS-Servers* is found in the directory from which the script is invoked, the contents will be added to the list of DNS Servers on the **Local Network** screen. (See below) 
- Enables VoLTE backup voice service and SMS reception.
- If run on the 18.1.c.0514 firmware, it will also add a button to access the pre-release version of DumaOS, but only if DumaOS has been enabled. To add this button, execute the [`dumaos-beta`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities) script with the `-on` parameter to enable DumaOS before running the `tch-gui-unhide-DJA0231-18.1.c.0514` script.

#### ipv4-DNS-Servers file
The contents of this file are simply the hostname and IP address, which are separated by a space. Multiple servers may be added, each on its own line.

For example, my *ipv4-DNS-Servers* file consists of my main and backup [Pi-hole](https://pi-hole.net/) servers:
```
Pi-hole 192.168.0.168
Pi-hole-VM 192.168.0.192
```
This adds these servers to the end of the IPv4 Primary and Secondary DNS lists on the **Local Network** screen.

## What does it actually do?
All configuration changes are applied using the *uci* command interface to update the configuration files.

GUI changes are implemented by making relatively small edits on the existing files.

No new executables are added to the system.

## How do I run it?
Basically, the steps are:

### First, you need to get root access to your device and ensure it is running the correct firmware:
Follow the instructions at https://hack-technicolor.rtfd.io

The scripts have only be tested against the specified hardware and firmware version denoted in the script name. If you are not running the firmware version for which the script was written, you should upgrade. (https://hack-technicolor.readthedocs.io/en/latest/Repository)

It is recommended that you apply whatever hardening (such as the [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities) script) and other configuration changes you want to make *before* executing the script, as features are enabled or disabled depending on the current configuration of the target device.

### Second, download and extract the scripts
Download and extract the release archive directly to the device with the following command:
```
 wget https://github.com/seud0nym/tch-gui-unhide/releases/latest/download/$(uci get env.var.variant_friendly_name)-$(uci get version.@version[0].version | cut -d- -f1).tar.gz -O - | tar -xzvf -
```
The above command will only work if run from a supported hardware variant and firmware combination, with internet access.

Alternatively, you can download the release to your computer and then upload it up to your device using WinSCP or equivalent. Run the `tar -xzvf <filename>` command to extract the release files.

### Third, optionally create customisation file (DJA0231 only)
- Create your *ipv4-DNS-Servers* file in the same directory as the scripts, as specified above.

### Last, execute the script
**PLEASE NOTE:** The GUI files will be restored to their original state (from /rom/www/...) by the script before it makes its modifications. If you have previously installed the Ansuel tch-nginx-gui, you should remove it before running this script.
#### DJA0231 with 18.1.c.0462 Firmware
```
./tch-gui-unhide-DJA0231-18.1.c.0462 <options>
```
#### DJA0231 with 18.1.c.0514 Firmware
```
./tch-gui-unhide-DJA0231-18.1.c.0514 <options>
```
#### TGA800vac with 17.2.0284 Firmware
```
./tch-gui-unhide-TG800vac-17.2.0284 <options>
```
The scripts accept the following options:
- -h hostname
    - Allows you to specify an alternate hostname. The default is determined by the hardware variant (i.e. either **DJA0231** or **TG800vac**).
- -d domain
    - Allows you to specify an alternate domain. The default is **gateway**.
- -t theme
    - By default, the script will apply a **light** theme. You may specify **night** to apply the dark theme. Specifying anything other than **night** will cause the light theme to be applied.
- -y
    - Allows you to skip the initial prompt to confirm execution, and automtically responds with **y**.
- -r
    - Allows you to revert the *GUI* changes. Configuration changes are **NOT** undone!
- -?
    - Displays usage information

The hardware and firmware version will be checked during execution. If they do not match the target version, you will be prompted to exit or force execution. This is **YOUR** decision to proceeed.

The script will restart and reload services for which it has modified configuration (except when the **-r** option is specified). Subsequent executions will not re-apply configuration already set correctly, and therefore will not restart services unnecessarily.

### Post-customisation configuration
You can remove specific cards/tabs from the GUI by changing the role in the web config. For example, I am on HFC, so xDSL is of no interest to me. So, I can remove the 'xDSL Config' card from the GUI by changing the allowed role from admin to something else using the following commands:
```
uci del_list web.xdsllowmodal.roles='admin'
uci add_list web.xdsllowmodal.roles='guest'
uci commit web
/etc/init.d/nginx restart
```
**WARNING:** Do not leave the roles list empty. It will break everything.

Taking this approach to customisation will prevent the card being displayed again if you re-run the script (unless you have cleared all GUI customisation by running the script with the -r option first).

### Finally, clear your browser cache
To see the updated logo and icons and to correctly apply the updated style sheet, you will probably need to clear cached images and files from your browser.

## Other Important Things To Note
- The script changes will not persist a reset or restore. If you reset your device, or restore to it a state before you applied the script, you will need to run the script again!

# Themes
*PLEASE NOTE: Previous releases of this script had additional scripts to switch between the light and dark themes. This release includes both themes in the single script.*

By default, the script will apply a "light" theme, similar to the DumaOS theme.

If you prefer a "dark" theme, run or re-run the script using the **-t night** option. To switch back, run again without the option or specify **-t light**.

After applying a theme, you will probably need to clear cached images and files from your browser to see the changes.

# Thanks
This would not have been possible without the following resources:

- https://hack-technicolor.rtfd.io
- https://forums.whirlpool.net.au/thread/9vxxl849
- https://github.com/Ansuel/tch-nginx-gui

Thank you to you all.
