# Utility Scripts
A collection of utility scripts for your Technicolor router.

## de-telstra
This script applies most of the hardening recommendations from https://hack-technicolor.readthedocs.io/en/stable/Hardening/, modified to only apply those that are applicable to later model devices such as the TG800vac and DJA0231. 

In addition, it:
- Overwrites the */etc/dropbear/authorized_keys* file with either:
    - the contents of an *authorized_keys* file found in the directory from which the script is invoked; or
    - an empty file (to remove any ISP default public keys)
- Correctly enables SSH LAN access
- Disables SSH WAN access
- Disables and removes CWMP
- Disables and removes Telstra monitoring
- Disables and removes Telstra AIR, including the SSIDs and GUI card
- Removes web GUI default user access
- Disables content and printer sharing, and the DLNA server
- Disables all ALGs except SIP (FTP, TFTP, SNMP, PPTP, IRC, AMANDA, RTSP)
- Removes packages related to CWMP, Telstra monitoring, and Telstra AIR

It doe NOT remove the hidden BH-xxxxxx SSID from the DJA0231, as this is not related to Telstra AIR. It is the wireless backhaul for EasyMesh.

## dumaos-beta
Enables or disables DumaOS on a DJA0231 running the 18.1.c.0514 firmware. It also disables or enables reboot on core dump, because if DumaOS gets into trouble, the router will just continally reboot.

This script supports 2 parameters:
- -on
    - Enables DumaOS and disables reboot on core dump, then starts the DumaOS service.
- -off
    - Disables DumaOS and enables reboot on core dump, then stops the DumaOS service.

If you enable DumaOS *after* running the `tch-gui-unhide-DJA0231-18.1.c.0514` script, you will need to re-run `tch-gui-unhide-DJA0231-18.1.c.0514` to enable the button to access DumaOS. Similarly, if you disable DumaOS, you will need to re-run `tch-gui-unhide-DJA0231-18.1.c.0514` to remove the button. 

## reset-to-factory-defaults-with-root
It does what it says. It is a copy of the commands from https://hack-technicolor.readthedocs.io/en/stable/Upgrade/#preserving-root-access (without the backup - you need to do that manually and move it off the device), with a confirmation prompt and immediate reboot.

This script supports 2 optional parameters:
- -f filename
    - Flash 'filename' into the mounted bank after reset and before reboot
- -n
    - Do NOT reboot.

## set-optimal-bank-plan
Again, it does what it says. It is a copy of the commands from https://hack-technicolor.readthedocs.io/en/stable/Hacking/PostRoot/#bank-planning, with a confirmation prompt.

## show-bank-plan
A pretty version of `find /proc/banktable -type f -print -exec cat {} ';'`, with a final analysis to show whether the bank plan is optimal, or not.

Example output:
```
/proc/banktable/active         : bank_1  OK
/proc/banktable/activeversion  : Unknown
/proc/banktable/booted         : bank_2  OK
/proc/banktable/bootedoid      : 5ed62a9e7b9c275f18e900f4
/proc/banktable/inactive       : bank_2
/proc/banktable/notbooted      : bank_1
/proc/banktable/notbootedoid   : Unknown
/proc/banktable/passiveversion : 18.1.c.0514-2881009-20200602123158-44c5de58631202876d970bdcc184a6c2dc8ef375

Bank Plan is OPTIMAL
```
## set-web-admin-password
Allows you to set or reset the web admin password. Specify the new password as the first parameter.

# How to download and execute these scripts
If you download the tch-gui-unhide release archive, the scripts are included.

You can also download the latest version individually:

**NOTE: Replace `<scriptname>` with the name of the script you wish to download.**

Execute these commands on your device via a PuTTY session or equivalent (an active WAN/Internet connection is required):
```
wget https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/utilities/<scriptname> 
```

Alternatively, download the script manually and load it up to your device using WinSCP or equivalent.

After you have the script on your device, you may need to make it executable, which is done with this command (assuming you are in the same directory as the script):
```
chmod +x <scriptname>
```

Then, execute the script (assuming you are in the same directory into which you downloaded or uploaded the script):
```
./<scriptname> <options>
```
