# Unlock the GUI on your Technicolor DJA0231
The *tch-gui-unhide-DJA0231-18.1.c.0514* script can be applied to the Telstra Smart Modem Gen 2 (Technicolor DJA0231) to unlock hidden features in the web GUI.

It also completely removes the Telstra AIR and FON Wi-Fi hotspot.

## Why not just use https://github.com/Ansuel/tch-nginx-gui?
When I first rooted my DJA0231, I applied the tch-nginx-gui because I had used it for some time on my Technicolor TG800vac, and found it very good.

However, on the DJA0231, I encountered various problems after reboots, such as loss of customised SSID's and IP addresses, loss of root in one instance, the admin password got reset to Telstra default, and so on.

So, I set out to enable whatever hidden features were included with the firmware by default, without making significant changes to the GUI code, and to try and make it almost as pretty as the tch-nginx-gui.

## What GUI features are unlocked?
- Configuration export/import, and the export file name is changed from *config.bin* to *DJA0231-18.1.c.0514-yymmdd.bin* (i.e it includes the source and the current date)
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
- The **CWMP** card is enabled to show and configure CWMP
- The **System Extras** card is enabled to:
    - Allow you to trigger a BOOTP upgrade (**BEWARE! Do not press this button unless you know what you are doing!**)
    - Turn SSH access on or off for the WAN/LAN
    - Configure an external syslog server
- The **NAT Helpers** card is enabled where you can enable or disable ALG's for FTP, IRC, SIP, PPTP, RTSP, SNMP, TFTP and AMANDA
- The **xDSL Config** card is enabled where you can see xDSL core settings

Some screens included on the device are not enabled, mainly because they fail and cause issues in the GUI, or sometimes more work would be required to implement them than can be done with simple stream editing of the existing files.

## What else does it do?
Basically, it applies most of the hardening recommendations from https://hack-technicolor.readthedocs.io/en/stable/Hardening/, modified to only apply those that are applicable to the DJA0231. 

It also does a few other things.

- You can set the hostname of the DJA0231 and the domain of the LAN. Default Telstra hostnames are removed.
- Overwrites the */etc/dropbear/authorized_keys* file with either:
    - the contents of an *authorized_keys* file found in the directory from which the script is invoked; or
    - an empty file (to remove any ISP default public keys)
- If a file called *ipv4-DNS-Servers* is found in the directory from which the script is invoked, the contents will be added to the list of DNS Servers on the **Local Network** screen. (See below) 
- Enables SSH LAN access
- Disables SSH WAN access
- Disables CWMP
- Disables Telstra monitoring
- Disables Telstra AIR, removes the associated SSIDs, and removes the GUI card
- Enables DumaOS
- Modernises the GUI a little bit 

#### ipv4-DNS-Servers
If a file called *ipv4-DNS-Servers* is found in the directory from which the script is invoked, the contents will be added to the list of DNS Servers on the **Local Network** screen.

The contents of the file are simply the one line per hostname and IP address, which are separated by a space.

For example, my *ipv4-DNS-Servers* file consists of my main and backup [Pi-hole](https://pi-hole.net/) servers:
```
Pi-hole 192.168.0.168
Pi-hole-VM 192.168.0.192
```

## What does it actually do?
All configuration changes are applied using the *uci* command interface to update the configuration files.

GUI changes are implemented by making relatively small edits on the existing files.

No new executables are added to the system.

## How do I run it?
### First, you need to get root access to your DJA0231 and ensure it is running the correct firmware:
Follow the instructions at https://hack-technicolor.rtfd.io

The script has only be tested against firmware version **18.1.c.0514-950-RB**. If you are not running this version, you should upgrade. (https://hack-technicolor.readthedocs.io/en/latest/Repository/#dja0231-vcnt-a)

### Second, download the script
Execute this command on your DJA0231 via a PuTTY session or eqivalent (an active WAN/Internet connection is required):

```
wget https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/tch-gui-unhide-DJA0231-18.1.c.0514 
```

Alternatively, download the script manually and load it up to your DJA0231 using WinSCP or equivalent.

You do **not** need to download any other files. The script contains all that is required (including images).

### Third, optionally create customisatiom files
- If you wish the script to apply your public keys to dropbear, place the *authorized_keys* file in the same directory as the script. **WARNING:** If no *authorized_keys* file is found in the current directory, */etc/dropbear/authorized_keys* will be **REPLACED** with an empty file!
- Create your *ipv4-DNS-Servers* file into the directory as the script.

### Last, execute the script
**PLEASE NOTE:** The GUI files will be restored to their original state (from /rom/www/...) by the script before it makes its modifications. If you have previously installed the Ansuel tch-nginx-gui, you should remove it before running this script.

The following command assumes you are in the same directory into which you downloaded or uploaded the script:
```
./tch-gui-unhide-DJA0231-18.1.c.0514
```

The script accepts 3 options:
- -h hostname
    - Allows you to specify an alternate hostname. The default is determined by the hardware variant (in this case, **DJA0231**).
- -d domain
    - Allows you to specify an alternate domain. The default is **gateway**.
- -r
    - Allows you to revert the *GUI* changes. Configuration changes are **NOT** undone!

The hardware and firmware version will be checked during execution. If they do not match the target version, you will be prompted to exit or force execution. This is **YOUR** decision to proceeed.

The script will restart and reload services for which it has modified configuration (except when the **-r** option is specified).

#### Post-customisation configuration
You can remove specific cards/tabs from the GUI by changing the role in the web config. For example, I am on HFC, so xDSL is of no interest to me. So, I can remove the 'xDSL Config' card from the GUI by changing the allowed role from admin to something else using the following commands:
```
uci del_list web.xdsllowmodal.roles='admin'
uci add_list web.xdsllowmodal.roles='guest'
uci commit web
/etc/init.d/nginx restart
```
**WARNING:** Do not leave the roles list empty. It will break everything.

Taking this approach to customisation will prevent the card being displayed again if you re-run *tch-gui-unhide-DJA0231-18.1.c.0514* (unless you have cleared all GUI customisation by running the script with the -r option first).

### Finally, clear your browser cache
To see the updated logo and icons and to correctly apply the updated style sheet, you will probably need to clear cached images and files from your browser.

# Themes
The script will apply a "light" theme, similar to the DumaOS theme.

If you prefer a "dark" theme, you can download and execute `night-theme-DJA0231-18.1.c.0514`. To revert to the light theme, you do not need to run `tch-gui-unhide-DJA0231-18.1.c.0514` again. You can just download and run `light-theme-DJA0231-18.1.c.0514` to reapply the light theme. Both of these scripts only apply the GUI changes. They do not modify configuration.

You do not need to download any other files. The theme scripts include all that they require, including the images.

After applying a theme, you will probably need to clear cached images and files from your browser to see the changes.

# Thanks
This would not have been possible without the following resources:

- https://hack-technicolor.rtfd.io
- https://forums.whirlpool.net.au/thread/9vxxl849
- https://github.com/Ansuel/tch-nginx-gui

Thank you to you all.
