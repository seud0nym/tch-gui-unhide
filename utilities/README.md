# Utility Scripts
A collection of utility scripts for your Technicolor router.

Download and execution instructions are [`below`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#how-to-download-and-execute-these-scripts).

## de-telstra
This script applies most of the hardening recommendations from https://hack-technicolor.readthedocs.io/en/stable/Hardening/. 

In addition, it automatically:
- Overwrites the */etc/dropbear/authorized_keys* file with either:
    - the contents of an *authorized_keys* file found in the directory from which the script is invoked; or
    - an empty file (to remove any ISP default public keys)
- Correctly enables SSH LAN access
- Disables and removes CWMP
- Disables and removes all Telstra monitoring and logging
- Disables and removes Telstra AIR, including the SSIDs and GUI card
- Removes web GUI default user access
- Sets SAMBA and DLNA host names to the device hostname (replacing Telstra-Modem)
- Removes packages related to CWMP, Telstra monitoring, and Telstra AIR

Optionally, it can also:
- Set the hostname of the device and the domain of the LAN. Default Telstra host names (except telstra.wifi, which is used by EasyMesh) are removed.
- Set the device DNS servers
- Disable or enable content and printer sharing, and the DLNA server
- Disable or enable all WAN ALGs (FTP, TFTP, SNMP, PPTP, IRC, AMANDA, RTSP, SIP)
- Disable or enable telephony, and enables VoLTE backup voice service and SMS reception
- Disable or enable DECT Emission Mode
- Disable or enable UPnP
- Disable or enable power saving
- Disable or enable MultiAP (EasyMesh) if it is installed on the device
- Disable or enable DumaOS (Telstra Game Optimiser) if it is installed on the device
- Disable or enable WPS on non-Guest and non-Backhaul SSIDs 
- Configure the opkg package manager so that you can install additional packages on the device
- Stop and disable all services associated with optional features for a minimal memory configuration
- Protect against loss of root when doing a factory reset, by preserving the password files and dropbear (SSH) configuration

It does NOT remove the hidden BH-xxxxxx SSID from the DJA0230 or DJA0231, as this is not related to Telstra AIR. It is the wireless backhaul for EasyMesh.
```
Usage: ./de-telstra [options]

Options:
Options:
 -k a|c|e|k|m|n        Override default hardening configuration:
    where a            - Keep Telstra AIR enabled
          c            - Keep CWMP installed
          k            - Keep default public authorized keys
          m            - Keep Telstra monitoring and data collection enabled
          n            - Keep Telstra NTP servers
          x            - Keep noexec on ext2/3/4, fat, hfsplus, hfsplusjournal and ntfs filesystems
                           (i.e. prevent execution of scripts/programs on USB devices)
          T            - Keep all default Telstra configuration (Equivalent to: -ka -kc -kk -km -kn -kx)
 -h u|d|s|<hostname>
    where u            Leave hostname unchanged
          d            Set the hostname to DJA0231
          s            Set the hostname to DJA0231-7E5F7A
          <hostname>   Use the specified hostname
 -d u|g|<domainname>
    where u            Leave domain name unchanged
          g            Set the domain name to gateway
          <domainname> Set the domain name to <domainname>
 -n u|a|c|g|f|o|<n.n.n.n>
    where u            Leave DNS servers unchanged
          a            Automatically use the DNS servers from the ISP
          c            Set the DNS servers to Cloudflare
          g            Set the DNS servers to Google
          f            Set the DNS servers to OpenDNS Family Shield
          o            Set the DNS servers to OpenDNS
          <n.n.n.n>    Set the DNS servers to 1 or 2 comma-separated IPv4 addresses (e.g. 8.8.8.8,1.1.1.1)
 -a u|y|n              WAN ALG NAT Helpers:     u=Unchanged y=Enable n=Disable
 -c u|y|n              Content Sharing:         u=unchanged y=Enable n=Disable
 -f u|y|n              File Sharing:            u=unchanged y=Enable n=Disable
 -p u|y|n              Power Saving:            u=unchanged y=Enable n=Disable
 -r u|y|n              Printer Sharing:         u=unchanged y=Enable n=Disable
 -t u|y|n              Telephony:               u=unchanged y=Enable n=Disable
 -e u|y|n              DECT Emission Mode:      u=unchanged y=Enable n=Disable
 -u u|y|n              UPnP Service:            u=unchanged y=Enable n=Disable
 -m u|y|n            * MultiAP (EasyMesh):      u=unchanged y=Enable n=Disable
 -g u|y|n            * DumaOS (Game Optimiser): u=unchanged y=Enable n=Disable
 -w u|y|n              WPS on non-Guest and non-Backhaul SSIDs: u=unchanged y=Enable n=Disable
 -F u|y|n              Factory reset root protection: u=unchanged y=Enable n=Disable
                          NOTE: Installation of tch-gui-unhide will ALWAYS enable RTFD protection!
 -A                    Equivalent to: -hd -dg -an -cn -fn -rn -un -wn -Fy
 -S                    Equivalent to: -hs -dg -an -cn -fn -rn -un -wn -Fy
 -M                    Minimum memory mode: Equivalent to: -an -cn -fn -rn -tn -en -un -mn -gn -Fy
                        PLUS stops and disables the associated services
 -R                    Reset to device defaults
                        (equivalent to: -h mymodem -d modem -na -ay -cy -fy -py -ry -ty -ey -uy -my -gy -wy -Fn)
 -o                    Configures opkg
 -U                    Download the latest version of de-telstra from GitHub
```
Note that the options to disable/enable EasyMesh and DumaOS are only applicable to devices with those services installed.

## dumaos
Enables or disables DumaOS on a DJA0231 running the 18.1.c.0514 or later firmware, or a DJA0230 running 18.1.c.0549 or later. It also disables or enables reboot on core dump, because if DumaOS gets into trouble, the router will just continually reboot.
```
Usage: ./dumaos -on|-off

Parameters:
 -on    Enables DumaOS and disables reboot on core dump, then starts the DumaOS service.
 -off   Disables DumaOS and enables reboot on core dump, then stops the DumaOS service.
```
If you enable DumaOS *after* running the `tch-gui-unhide` script, you will need to re-run `tch-gui-unhide` to enable the button to access DumaOS. Similarly, if you disable DumaOS, you will need to re-run `tch-gui-unhide` to remove the button. 

## intercept-dns
Configures DNS interception:
- Hijacks IPv4 DNS requests to ensure that they are handled by the router, or by a specified DNS Server
- Rejects DNS-over-TLS (DoT) requests over IPv4 and IPv6
- Rejects DNS-over-HTTPS (DoH) to known HTTPS DNS Servers over IPv4 and IPv6
- Configures a scheduled weekly cron job to maintain IP Sets of known HTTPS DNS Servers

This script is based upon the configuration specified in https://openwrt.org/docs/guide-user/firewall/fw3_configurations/intercept_dns, with modifications to support the OpenWRT version in use on the Telstra Technicolor devices.
```
Usage: ./intercept-dns [options]

Options:
 -d n.n.n.n   The IPv4 address of the local DNS Server to which DNS queries will be redirected.
                If not specified, defaults to the router.
 -x n.n.n.n   Exclude the specified IPv4 address from DNS interception. May be specified multiple times
                to exclude multiple IPv4 addresses.
                The local DNS Server specified with -d is automatically excluded and does not need to
                be re-specified with -x. 
 -6           Do NOT apply blocking to IPv6 DNS requests.
 -r           Remove DNS interception.
 -U           Download the latest version of intercept-dns rom GitHub
```
The list of known DoH hosts is retrieved from https://github.com/dibdot/DoH-IP-blocklists.

*Please note* that, as the Telstra Technicolor devices do not have the `kmod-ipt-nat6` kernel module installed, DNS hijacking is **NOT** possible for IPv6. Blocking DoT and DoH **IS** supported for IPv6, because they rely on blocking a specific port for DoT and the IP Set of known DoH hosts.

## mtd-backup
Backs up the MTD partitions to an attached USB device or SSHFS attached filesystem. Only unchanged partitions are backed up after the first execution.
```
Usage: ./mtd-backup [options]

Options:
 -d directory   The name of the directory on the USB device or SSHFS filesystem.
                  If not specified, defaults to: backups
 -c             Save the current UCI configuration into the DJA0231-CP1837TA46D-config.gz file
 -e             Save the current environment into the DJA0231-CP1837TA46D-env file
 -l             Write log messages to stderr as well as the system log
 -o             Save the overlay content into the DJA0231-CP1837TA46D-overlay-files-backup.tgz file
 -v             Verbose mode
 -y             Bypass confirmation prompt (answers 'y')
 -C             Adds or removes the scheduled daily backup cron job
 -P             Reports the backup path
 -U             Download the latest version of mtd-backup from GitHub
```
When run with the -C option (which should be the only option), the scheduled job will be added if it does not already exist, or removed if it does exist in the schedule. By default, the backup will run every day at a random time between 2:00am and 5:00am. You can modify the schedule through the Management card in `tch-gui-unhide`, or by directly modifying the /etc/crontab/root file.

## mtd-restore
Restores MTD partitions from an attached USB device or SSHFS filesystem. Only changed partitions are restored.
```
Usage: ./mtd-restore [options] [partition ...]

Options:
 -d directory   The name of the directory on the USB device or SSHFS filesystem.
                  If not specified, defaults to: backups
 -U             Download the latest version of $SCRIPT from GitHub
                  Do NOT specify any other parameters or options if doing a version upgrade.
Parameters:
 partition      One or more partitions to restored.
                  Specify either the device (e.g. "mtd2") or name (e.g. "rootfs_data")
                  If not specified, defaults to VARIANT-mtd2-rootfs_data

```
If no partition is specified for restore, ALL partitions that have been altered will be restored.

## reboot-on-coredump
Enables or disables reboot on core dump. If you have a process that is continually crashing and core dumping, use this script to disable reboot on coredump.
```
Usage: ./reboot-on-coredump -on|-off

Parameters:
 -on    Enables reboot on core dump.
 -off   Disables reboot on core dump.
```

## reset-to-factory-defaults-with-root
It does what it says. It is a copy of the commands from https://hack-technicolor.readthedocs.io/en/stable/Upgrade/#preserving-root-access, with a confirmation prompt and immediate reboot.
```
Usage: ./reset-to-factory-defaults-with-root [options]

Options:
 -b           Make a full backup of your booted bank configuration (requires attached USB device).
 -c           Disable CWMPD configuration during first boot after reset.
 -f filename  Flash 'filename' into the mounted bank after reset and before reboot,
                If 'filename' ends with .rbi, it will be unpacked first, either to an attached USB device, or /tmp if no USB detected.
 -I n.n.n.n   Use IP address n.n.n.n after reset and reboot.
 -i           Keep the existing IP address after reset and reboot.
 -n           Do NOT reboot.
 -U           Download the latest version of reset-to-factory-defaults-with-root rom GitHub
```

## set-optimal-bank-plan
Again, it does what it says. It is a copy of the commands from https://hack-technicolor.readthedocs.io/en/stable/Hacking/PostRoot/#bank-planning, with a confirmation prompt.
```
Usage: ./set-optimal-bank-plan
```

## set-web-admin-password
Allows you to set or reset the web admin password. Specify the new password as the first parameter.
```
Usage: ./set-web-admin-password newpassword

Parameters:
 newpassword   the new admin password for the GUI. (Required)
```

## show-bank-plan
A pretty version of `find /proc/banktable -type f -print -exec cat {} ';'`, with a final analysis to show whether the bank plan is optimal, or not.
```
Usage: ./show-bank-plan
```
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
## transformer-cli
Version 17 firmwares do not include `/usr/bin/transformer-cli`, which is very useful for working out what is returned in the various GUI scripts.

## unpack-rbi
Unpacks the *.rbi* file passed as the first parameter. 
```
Usage: ./unpack-rbi source [target]

Parameters:
 source   the .rbi file to be unpacked. (Required)
 target   either a filename or directory to which the .rbi file will be unpacked. (Optional)
```
The second parameter is optional, and is either a filename or directory to which the *.rbi* file will be unpacked. If the filename or directory is not specified, the file will be written to the /tmp directory with the same name but with an extension of *.bin*.

## update-ca-certificates
Downloads and installs the latest version of the System CA Certificates. 
```
Usage: ./update-ca-certificates [options]

Options:
 -C  Adds or removes the scheduled monthly cron job
 -U  Download the latest version of update-ca-certificates from GitHub
```

# How to download and execute these scripts
If you download a tch-gui-unhide release archive, the scripts applicable to that firmware version are included.

You can also download the latest version individually:

**NOTE: Replace `<scriptname>` with the name of the script you wish to download.**

Execute this command on your device via a PuTTY session or equivalent (an active WAN/Internet connection is required):
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
