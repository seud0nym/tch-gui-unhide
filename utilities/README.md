# Utility Scripts
A collection of utility scripts for your Technicolor router. Most of the names are self-explanatory.

- [de-telstra](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra)
- [dumaos](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#dumaos)
- [guest-restore](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#guest-restore)
- [hijack-dns](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#hijack-dns)
- [log-check](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#log-check)
- [move-lan-port-to-own-network](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#move-lan-port-to-own-network)
- [mtd-backup](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#mtd-backup)
- [mtd-restore](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#mtd-restore)
- [overlay-restore](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#overlay-restore)
- [ptree](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#ptree)
- [reboot-on-coredump](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#reboot-on-coredump)
- [reset-to-factory-defaults-with-root](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#reset-to-factory-defaults-with-root)
- [safe-firmware-upgrade](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#safe-firmware-upgrade)
- [set-optimal-bank-plan](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#set-optimal-bank-plan)
- [set-web-admin-password](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#set-web-admin-password)
- [show-bank-plan](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#show-bank-plan)
- [transformer-cli](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#transformer-cli)
- [unpack-rbi](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#unpack-rbi)
- [update-ca-certificates](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#update-ca-certificates)

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
- Set the device IPv4 DNS servers
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
- Completely remove the Guest Wi-Fi SSIDs, firewall rules/zones, and guest networks

It does NOT remove the hidden BH-xxxxxx SSID, as this is not related to Telstra AIR. It is the wireless backhaul for EasyMesh.
```
Usage: ./de-telstra [options]

Options:
 -k a|c|e|k|m|n|s|x    Override default hardening configuration:
    where a            - Keep Telstra AIR enabled
          c            - Keep CWMP installed
          k            - Keep default public authorized keys
          l            - Keep Telstra APN check
          m            - Keep Telstra monitoring and data collection enabled
          n            - Keep Telstra NTP servers
          q            - Keep Telstra QoS VoWiFi reclassify rules
          s            - Keep WAN Supervision as BFD
          x            - Keep noexec on ext2/3/4, fat, hfsplus, hfsplusjournal
                          and ntfs filesystems (i.e. prevent execution of
                          scripts/programs on USB devices)
          T            - Keep all default Telstra configuration (Equivalent
                          to: -ka -kc -kk -kl -km -kn -kq -ks -kx)
 -h u|d|s|<hostname>
    where u            Leave hostname unchanged
          d            Set the hostname to VARIANT (e.g. DJA0231)
          s            Set the hostname to VARIANT-MAC_HEX (e.g. DJA0231-XXXXXX)
          <hostname>   Use the specified hostname
 -d u|g|l|<domainname>
    where u            Leave domain name unchanged
          g            Set the domain name to gateway
          l            Set the domain name to lan
          <domainname> Set the domain name to <domainname>
 -n u|a|c|g|f|o|<n.n.n.n>
    where u            Leave DNS servers unchanged
          a            Automatically use the DNS servers from the ISP
          c            Set the DNS servers to Cloudflare
          g            Set the DNS servers to Google
          f            Set the DNS servers to OpenDNS Family Shield
          o            Set the DNS servers to OpenDNS
          <n.n.n.n>    Set the DNS servers to 1 or 2 comma-separated
                        IPv4 addresses (e.g. 8.8.8.8,1.1.1.1)
 -a u|y|n              NAT ALG Helpers:         u=Unchanged y=Enable n=Disable
 -c u|y|n              Content Sharing:         u=unchanged y=Enable n=Disable
 -e u|y|n              DECT Emission Mode:      u=unchanged y=Enable n=Disable
 -f u|y|n              File Sharing:            u=unchanged y=Enable n=Disable
 -g u|y|n            * DumaOS (Game Optimiser): u=unchanged y=Enable n=Disable
 -i u|y|n              Intercept Daemon:        u=unchanged y=Enable n=Disable
 -l u|y|n            * LED logging:             u=unchanged y=Enable n=Disable 
 -m u|a|b|c|v|y|n    * MultiAP (EasyMesh):      u=unchanged a=Enable Agent
                                                            b=Enable BackHaul SSID
                                                            c=Enable Controller 
                                                            v=Enable Vendor Extensions
                                                            y=same as -ma -mb -mc -mv
                                                            n=Disable
 -p u|y|n|d            Power Saving:            u=unchanged y=Enable n=Disable d=Default
 -q u|y|n            * NFC:                     u=unchanged y=Enable n=Disable
 -r u|y|n              Printer Sharing:         u=unchanged y=Enable n=Disable
 -s u|b|d|n            WAN Supervision:         u=unchanged b=BFD d=DNS n=Disable
 -t u|y|n|m            Telephony:               u=unchanged y=Enable n=Disable
                                                m=Enable w/o services (ex. HOLD + DND)
 -u u|y|n              UPnP Service:            u=unchanged y=Enable n=Disable
 -w u|y|n              WPS:                     u=unchanged y=Enable n=Disable
                         (on non-Guest and non-Backhaul SSIDs)
 -F u|y|n              RTFD root protection:    u=unchanged y=Enable n=Disable
                         NOTE: tch-gui-unhide will ALWAYS enable RTFD protection
 -I n.n.n.n            Set the LAN IPv4 address to n.n.n.n
 -A                    Equivalent to: -hd -dg -an -cn -fn -ln -in -rn -sd -un -wn -Fy
 -S                    Equivalent to: -hs -dg -an -cn -fn -ln -in -rn -sd -un -wn -Fy
 -M                    Minimum memory mode: Equivalent to:
                           -an -cn -fn -in -rn -tn -en -un -mn -gn -qn -Fy
                         PLUS stops and disables the associated services
 -G                    Removes the Guest Wi-Fi SSIDs, firewall rules/zones, and guest networks
 -R                    Reset to device defaults: Equivalent to:
                         -h mymodem -d modem -na -ay -cy -fy -iy -pd -ry -ty -ey -uy -my -gy -qy -wy -Fn -sb
 -o                    Configures opkg
 -O 17|18|19           Overrides the default opkg repository with the specified version
 -U                    Download the latest version of de-telstra from GitHub
 -y                    Bypass the confirmation prompt (answers 'y')
 --save-defaults       Saves the command line options as defaults for future executions
                         When specified, NO changes are applied to the device
 --show-defaults       Shows the settings that would be applied (defaults and over-rides)
                         When specified, NO changes are applied to the device
 --no-defaults         Ignores any saved defaults for this execution
 --no-service-restart  Do NOT restart services after applying configuration changes
 --no-password-remind  Do NOT remind to change root password
```
#### Notes
1. The default for all optional parameters is u (unchanged).
2. The options to disable/enable NFC, EasyMesh and DumaOS are only applicable to devices with those services installed.
3. Shortcut options (e.g. *-A*, *-S*, *-M* and *-R*) can have their settings overridden by specifying the required option **AFTER** the shortcut option. For example, the *-A* option disables Content Sharing (*-cn*). However, you can specify *-A -cy* to enable Content Sharing and still apply all the other options implied by *-A*.

#### Warning
* Enabling power saving on FW 20.3.c will power down WAN/LAN ports!!

## dumaos
Enables or disables DumaOS on a DJA0231 running the 18.1.c.0514 or later firmware, or a DJA0230 running 18.1.c.0549 or later. It also disables or enables reboot on core dump, because if DumaOS gets into trouble, the router will just continually reboot.
```
Usage: ./dumaos -on|-off

Parameters:
 -on    Enables DumaOS and disables reboot on core dump, then starts the DumaOS service.
 -off   Disables DumaOS and enables reboot on core dump, then stops the DumaOS service.
```
If you enable DumaOS *after* running the `tch-gui-unhide` script, you will need to re-run `tch-gui-unhide` to enable the button to access DumaOS. Similarly, if you disable DumaOS, you will need to re-run `tch-gui-unhide` to remove the button. 

## guest-restore
Restores the Guest Wi-Fi SSIDs, firewall rules/zones, and guest networks if they were removed using the de-telstra -G option.
```
Usage: ./guest-restore [options]

Options:
 -2   Do NOT restore the 2.4GHz Guest Wi-Fi SSID
 -5   Do NOT restore the 5GHz Guest Wi-Fi SSID
 -e   Enable Guest Wi-Fi
        (Default is to leave Guest Wi-Fi disabled)
 -r   Reboot after applying configuration
 -v   Show verbose messages
 -y   Bypass the confirmation prompt (answers "y")
 -U   Download the latest version of guest-restore from GitHub
```

## hijack-dns
Configures DNS hijacking:

 - Hijacks DNS requests to ensure that they are handled by the device, or by a specified DNS Server
 - Rejects DNS-over-TLS (DoT) requests over IPv4 and IPv6
 - Rejects DNS-over-HTTPS (DoH) to known HTTPS DNS Servers over IPv4 and IPv6
 - Configures a scheduled weekly cron job to maintain IP Sets of known HTTPS DNS Servers
```
Usage: ./hijack-dns [options]

Options:
 -4 y|n       Enables IPv4 DNS hijacking. Defaults to y if not specified.
 -6 y|n       Enables IPv6 DNS hijacking. Defaults to y if not specified.
 -d <ip_addr> The IPaddress of the local DNS Server to which DNS queries
                will be redirected. You can specify this option twice: once 
                for an IPv4 address and a second time for an IPv6 address.
                If either is not specified, it defaults to the router. 
 -x <ip_addr> Exclude the specified IPv4 or IPv6 address from being 
                hijacked. You may specify this option multiple times to 
                exclude multiple IP addresses. The local DNS Server specified
                with -d is automatically excluded and does not need to be 
                re-specified with -x.
 -r           Disables DNS hijacking. Specify twice to remove configuration
                and three times to remove support files. Removing the
                configuration and support files is NOT recommended. They will
                also be recreated automatically by tch-gui-unhide.
 --status     Shows and verifies DNS hijacking status
 --fix        Only applicable with --status. Attempts to correct errors.
 -U           Download the latest version of the script and supporting files
                from GitHub
```

_NOTE_: **tch-gui-unhide releases starting with 2021.08.18 allow you to enable and configure DNS hijacking through a tab on the Firewall, and releases starting with 2022.01.01 support DNS hijacking on IPv6 using a transparent proxy.** 

## intercept-dns
*The intercept-dns script has been renamed to [hijack-dns](#hijack-dns) to avoid confusion with the intercept daemon.*

## log-check
Checks that the logging RAM buffer and disk-based messages log are both being written correctly.
```
Usage: ./log-check [options]

Options:
 -C    Adds or removes the scheduled hourly cron job
```

## move-lan-port-to-own-network
Moves a LAN port from the LAN bridge to its own network.
```
Usage: ./move-lan-port-to-own-network [options]

Options:
 -p 2|3|4     Specifies the LAN port to be moved to its own network. Use of LAN
                port 1 is not supported. When used in conjunction with -R, 
                specifies the port to be moved back to the LAN bridge. If not 
                specified, defaults to '4'.
 -n name      The interface name of the network. This is the name that will 
                appear in the Local Network screen to manage the network. If not
                specified, defaults to 'Port'+port number.
 -i n.n.n.n   Specifies the IP address to be assigned to the LAN port. Cannot be
                in an existing IP range used by other networks (e.g. LAN. Guest).
                If not specified, defaults to '192.168.3.1'.
 -m n.n.n.n   Specifies the subnet mask to be applied to the new network. If not 
                specified, defaults to '255.255.255.0'.
                NOTE: Only 255.255.255.0 is currently supported.
 -6 n         Specifies the IPv6 hint to be assigned to the new network. Use 'n' 
                to disable IPv6 on this network. If not specified, defaults to 
                the next available hint.
 -d domain    The domain name to use for the new network. If not specified, 
                defaults to 'modem'.
 -h hostname  The host name to be associated with the IP address. If not 
                specified, defaults to the same as the LAN hostname.
 -f g|g5|l|c  Add the new network into the specified firewall zone:
                  g  = Guest
                  g5 = 5GHz Guest (same as g on FW 20.4)
                  l  = LAN
                  c  = Create new firewall zone
                If not specified, defaults to g (Guest), unless it does not exist,
                in which case it will be assigned to the l (LAN) zone.
 -R           Restore the LAN port to the LAN bridge.
 -v           Show verbose messages
 -y           Bypass the confirmation prompt (answers "y")
 -U           Download the latest version of move-lan-port-to-own-network from GitHub
```

## mtd-backup
Backs up mtd or ubifs device partitions to an attached USB device or SSHFS attached filesystem. Only unchanged partitions are backed up after the first execution.

USB devices have priority over SSHFS filesystems. 
```
Usage: ./mtd-backup [options]

Options:
 -d directory    The name of the directory on the USB device or SSHFS 
                   filesystem.
                   If not specified, defaults to: backups
 -c              Save the current UCI configuration into the 
                   VARIANT-SERIAL-VERSION-config.gz file
 -e              Save the current environment into the 
                   VARIANT-SERIAL-VERSION-env file
 -o              Save the overlay content into the 
                   VARIANT-SERIAL-VERSION-overlay-files-backup.tgz file
 -s              Skip recalculation of the checksum of the backed-up 
                   partition, and just save the checksum calculated 
                   to determine if the image has changed.
 -0              Skip backup of mtd0 if a backup already exists 
                   (Ignored for UBIFS partitions)
 --no-drop-cache Skips flushing the RAM page cache after backup
 -l              Write log messages to stderr as well as the system log
 -v              Verbose mode
 -y              Bypass confirmation prompt (answers 'y')
 -C              Adds or removes the scheduled daily backup cron job
 -P              Reports the backup path
 -U              Download the latest version of mtd-backup from GitHub
                   Do NOT specify any other parameters or options if 
                   doing a version upgrade.
```
When run with the -C option (which should be the only option), the scheduled job will be added if it does not already exist, or removed if it does exist in the schedule. By default, the backup will run every day at a random time between 2:00am and 5:00am. You can modify the schedule through the Management card in `tch-gui-unhide`, or by directly modifying the /etc/crontab/root file.

## mtd-restore
Restores mtd or ubifs partitions from an attached USB device or SSHFS filesystem. Only changed partitions are restored (unless -s is specified).

USB devices have priority over SSHFS filesystems.

Due to the nature of the UBI filesystem, restores will fail unless the target partition is not mounted.
```
Usage: ./mtd-restore [options] [partition ...]

Options:
 -b             When restoring firmware bank partitions, swap the banks
                  (e.g. if restoring bank_1, write it into bank_2) 
 -d directory   The name of the directory on the USB device or SSHFS
                  filesystem containing the backups. If not specified,
                  defaults to: backups or backups-FIRMWARE_MAJOR.MINOR
                  (e.g. backups-20.3)
 -q             Quiet mode
 -r             Reboot after last partition successfully restored
 -s             Skip check for changed partitions and always restore
 -v             Skip verification of image checksum (if it exists i.e. if
                  mtd-backup was NOT executed with the -s option)
 -U             Download the latest version of mtd-restore from GitHub
                  Do NOT specify any other parameters or options if doing
                  a version upgrade.
 --dry-run      Show restore commands rather than executing them
Parameters:
 partition      One or more partitions to restored.
                  Specify either the device (e.g. "mtd2") or name (e.g. "rootfs_data")
                  Do not specify the device variant prefix (e.g. DJA0231-)
                  If not specified, defaults to: rootfs_data
```

## overlay-restore
Restores an overlay tar backup, such as those created by mtd-backup, reset-to-factory-defaults-with-root, and safe-firmware-upgrade. 

By default, both bank_1 and bank_2 will be restored.
```
Usage: ./overlay-restore [options] [filename]

Options:
 -1             Only restore the bank_1 overlay
 -2             Only restore the bank_2 overlay
 -b             Only restore the booted bank overlay
 -p             Only restore the not booted bank overlay
 -v             Verbose mode (list files as they are restored)
 -R             Do NOT reboot after restore is completed
                  This is the default if only the not booted bank 
                  is being restored.
 -U             Download the latest version of $SCRIPT from GitHub
                  Do NOT specify any other parameters or options 
                  if doing a version upgrade.
Parameters:
 filename      The filename containing the /overlay tar backup. If not specified, defaults to: 
                  /mnt/usb/FIRST USB DEVICE/backups/VARIANT-SERIAL-VERSION-overlay-files-backup.tgz
```

## ptree
Similar to the `ps` command, but shows child processes in a tree-like structure.
```
Usage: ./ptree
```

## reboot-on-coredump
Enables or disables reboot on core dump. If you have a process that is continually crashing and core dumping, use this script to disable reboot on coredump.
```
Usage: ./reboot-on-coredump -on|-off

Parameters:
 -on    Enables reboot on core dump.
 -off   Disables reboot on core dump.
```

## reset-to-factory-defaults-with-root
This script implements the commands from https://hack-technicolor.readthedocs.io/en/stable/Upgrade/#preserving-root-access to reset the device to factory defaults whilst preserving root access.
```
Usage: ./reset-to-factory-defaults-with-root [options]

Options:
 -b               Make a full backup of your configuration from /overlay 
                    before resetting to factory defaults.
                    (Requires attached USB device).
 -c               Disable CWMP configuration during first boot after reset.
 -d               Add DNS rewrites to disable CWMP firmware downloads from
                    fwstore.bdms.telstra.net
 -D domain        Add DNS rewrites to disable CWMP firmware downloads from
                    the specified domain. May be specified multiple times.
 -e               Disable any 'noexec' flags on USB mounted filesystems.
 -f filename      Flashes the specified firmware 'filename' before reset and 
                    reboot. If 'filename' ends with .rbi, it will be unpacked 
                    first, either to an attached USB device, or /tmp if no USB 
                    is detected. 
                    - If 'filename' ends in .rbi or .bin, it will be flashed 
                      into the booted bank, unless -s is specified.
                    - If 'filename' ends with .pkgtb, the firmware will be 
                      flashed into the passive bank using sysupgrade (root 
                      access will be preserved) and banks will be switched on 
                      reboot.
 -h d|s|hostname  Sets the device hostname, where:
                    d = Set the hostname to VARIANT
                    s = Set the hostname to VARIANT-MAC_HEX
                    hostname = Use the specified hostname
 -i               Keep the existing LAN IP address after reset and reboot.
                    This is the default if --restore-config is specified.
                    By default, also restores port forwards, static leases
                    and the IPv6 ULA and prefix size (unless --no-forwards, 
                    --no-leases or --no-ula are specified).
 -I n.n.n.n       Use IP address n.n.n.n after reset and reboot.
 -k               Keep existing SSH keys after reset and reboot.
 -l n.n.n.n:port  Configure logging to a remote syslog server on the specified
                    IP address and port. The port is optional and defaults to
                    514 if not specified.
 -m               Keep existing mobile operators and profiles, and linked 
                    WWAN profile.
                    Ignored if no mobile profiles found.
 -n               Do NOT reboot.
 -p password      Set the password after reset and reboot. If not specified,
                    it defaults to root.
 -s               Apply factory reset with root to the not booted bank, rather 
                    than the booted bank, and then switch banks after reboot.
                    Firmware will also be flashed into the passive bank.
                    This is the default when flashing a .pkgtb firmware into 
                    the passive bank.
 -v               Show the reset script after it has been written.
 -y               Bypass confirmation prompt (answers 'y').
 --no-forwards    Bypass restore of port forwards (ignored unless -i is
                    specified).
 --no-leases      Bypass restore of static leases (ignored unless -i is
                    specified).
 --no-ula         Bypass restore of the IPv6 ULA and LAN prefix size (ignored 
                    unless -i is specified).
 --save-defaults  Saves the command line options (except -f/-s/-y) as defaults.
                    When specified, NO changes are applied to the device.
 --no-defaults    Ignores any saved defaults for this execution.
                    --no-defaults must be the FIRST option specified.
 -U               Download the latest version of the script from GitHub.
                    Do NOT specify any other parameters or options if doing a 
                    version upgrade.
 --restore-config Runs the restore-config.sh script after reboot if it is found
                    in the USB backups directory. Output will be written to the 
                    system log. --restore-config should be the LAST option
                    specified, and may optionally be followed by the name of
                    the overlay backup file to be restored. Saved defaults are
                    IGNORED when --restore-config is specified.
 --i              Specifies that the IP address configured by the -i or -I options 
                    is also to be applied after the configuration is restored. If
                    not specified, the IP address used will be the one found in the 
                    configuration backup. Ignored unless --restore-config is also 
                    specified.
```

## safe-firmware-upgrade
Applies a new firmware to the device, without losing root access.

It is basically the same as the procedure as described in
http://hack-technicolor.rtfd.io/en/stable/Upgrade/#preserving-root-access and
http://hack-technicolor.rtfd.io/en/stable/Upgrade/#flashing-firmware
but with some additional options.

This script has a dependency on the `reset-to-factory-defaults-with-root` script. If that script does not exist, or is not the correct version, it will be downloaded as needed.
```
Usage: ./safe-firmware-upgrade [options] filename

Where:
 filename         Is the name of the firmware file to be flashed. If the 
                    filename ends with .rbi, it will be unpacked first, 
                    either to an attached USB device, or /tmp if no USB is 
                    detected. 
                    - If 'filename' ends in .rbi or .bin, it will be flashed 
                      into the booted bank
                    - If 'filename' ends with .pkgtb, the firmware will be 
                      flashed into the passive bank using sysupgrade (root 
                      access will be preserved) and banks will be switched 
                      on reboot.

Options:
 -b               Make a full backup of your configuration from /overlay
                    (Requires attached USB device).
 -c               Disable CWMP configuration during first boot after reset.
 -d               Add DNS rewrites to disable CWMP firmware downloads from
                    fwstore.bdms.telstra.net
 -D domain        Add DNS rewrites to disable CWMP firmware downloads from
                    the specified domain. May be specified multiple times.
 -e               Disable any 'noexec' flags on USB mounted filesystems.
 -h d|s|hostname  Sets the device hostname, where:
                    d = Set the hostname to VARIANT
                    s = Set the hostname to VARIANT-MAC_HEX
                    hostname = Use the specified hostname
 -i               Keep the existing LAN IP address after reset and reboot.
                    This is the default if --restore-config is specified.
                    By default, also restores port forwards, static leases
                    and the IPv6 ULA and prefix size (unless --no-forwards, 
                    --no-leases or --no-ula are specified).
 -I n.n.n.n       Use IP address n.n.n.n after reset and reboot.
 -k               Keep existing SSH keys after reset and reboot.
 -l n.n.n.n:port  Configure logging to a remote syslog server on the specified
                    IP address and port. The port is optional and defaults to
                    514 if not specified.
 -m               Keep existing mobile operators and profiles, and linked 
                    WWAN profile.
                    Ignored if no mobile profiles found.
 -n               Do NOT reboot.
 -p password      Set the password after reset and reboot. If not specified,
                    it defaults to root.
 -v               Show the reset script after it has been written.
 -y               Bypass confirmation prompt (answers 'y')
 --no-forwards    Bypass restore of port forwards (ignored unless -i is
                    specified).
 --no-leases      Bypass restore of static leases (ignored unless -i is
                    specified).
 --no-ula         Bypass restore of the IPv6 ULA and LAN prefix size (ignored 
                    unless -i is specified).
 --save-defaults  Saves the command line options (except filename/-s/-y) as 
                    defaults.
                    When specified, NO changes are applied to the device.
 --no-defaults    Ignores any saved defaults for this execution.
                    --no-defaults must be the FIRST option specified.
 -U               Download the latest version of the script from GitHub.
                    Do NOT specify any other parameters or options if doing a
                    version upgrade.
 --restore-config Runs the restore-config.sh script after reboot if it is found
                    in the USB backups directory. Output will be written to the 
                    system log. --restore-config should be the LAST option
                    specified, and may optionally be followed by the name of
                    the overlay backup file to be restored. Saved defaults are
                    IGNORED when --restore-config is specified.
 --i              Specifies that the IP address configured by the -i or -I options 
                    is also to be applied after the configuration is restored. If
                    not specified, the IP address used will be the one found in the 
                    configuration backup. Ignored unless --restore-config is also 
                    specified.
```

## set-optimal-bank-plan
It is a copy of the commands from https://hack-technicolor.readthedocs.io/en/stable/Hacking/PostRoot/#bank-planning, with a confirmation prompt.
```
Usage: ./set-optimal-bank-plan
```

## set-web-admin-password
Allows you to set or remove the web admin password.
```
Usage: ./set-web-admin-password [-d|<password>]

Parameters:
 -d              Enables the default user so that no
                  password is required for the GUI.
 <password>      The new admin password for the GUI. 
                  (Required if -d is not specified, 
                  ignored when -d is specified.)
```

## show-bank-plan
A pretty version of `find /proc/banktable -type f -print -exec cat {} ';'` (on firmware up to 20.3.c) or the output from `bootmgr` on firmware 20.4, with a final analysis to show whether the bank plan is optimal, or not.

The output also indicates if the booted firmware is vulnerable to [tch-exploit](https://github.com/BoLaMN/tch-exploit) or not.
```
Usage: ./show-bank-plan [-q]
```
Example output:
```
 -> active           : bank_1  OK
 -> activeversion    : Unknown
 -> booted           : bank_2  OK
 -> bootedoid        : 62306870a05c1444ac3737f6
 -> inactive         : bank_2
 -> notbooted        : bank_1
 -> notbootedoid     : Unknown
 -> passiveversion   : 20.3.c.0432-3241006-20220315112032-f4cc8d43fdb9e1fe7bf3bba5aa8caddbc8cb4014

!! WARNING: Booted firmware is NOT VULNERABLE to tch-exploit !!

 == Bank Plan is OPTIMAL ==
```

The script returns 0 if the bank plan is optimal, and 1 if it is not.

If run with the `-q` option, then no output is displayed, and you must rely on the return code.

## transformer-cli
Version 17 firmware does not include `/usr/bin/transformer-cli`, which is very useful for working out what is returned in the various GUI scripts.

## unpack-rbi
Unpacks the *.rbi* file passed as the first parameter. 
```
Usage: ./unpack-rbi source [target]

Parameters:
 source   the .rbi file to be unpacked. (Required)
 target   either a filename or directory to which the .rbi file will be unpacked. (Optional)
```
The second parameter is optional, and is either a filename or directory to which the *.rbi* file will be unpacked. If the filename or directory is not specified, the file will be written to either a mounted USB device, or if no USB device is found, the /tmp directory. The file will have the same name but will have an extension of '.bin'.

## update-ca-certificates
Downloads and installs the latest version of the System CA Certificates. 
```
Usage: ./update-ca-certificates [options]

Options:
 -v  Verbose mode
 -C  Adds or removes the scheduled monthly cron job
 -U  Download the latest version of update-ca-certificates from GitHub
```

# How to download and execute these scripts
If you download a tch-gui-unhide release archive, the scripts applicable to that firmware version are included.

You can also download the latest version individually:

**NOTE: Replace `<script>` with the name of the script you wish to download.**

Execute this command on your device via a PuTTY session or equivalent (an active WAN/Internet connection is required):
```
wget https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/utilities/<script> 
```

Alternatively, download the script manually and load it up to your device using WinSCP or equivalent.

After you have the script on your device, you may need to make it executable, which is done with this command (assuming you are in the same directory as the script):
```
chmod +x <script>
```

Then, execute the script (assuming you are in the same directory into which you downloaded or uploaded the script):
```
./<script> <options>
```
