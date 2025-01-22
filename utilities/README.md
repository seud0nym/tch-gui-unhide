# Utility Scripts
A collection of utility scripts for your Technicolor router. Most of the names are self-explanatory.

- [de-telstra](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra)
- [dumaos](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#dumaos)
- [guest-restore](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#guest-restore)
- [log-check](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#log-check)
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
- [unflock](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#unflock)
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
- Disable or enable telephony
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
 -a u|y|n              NAT ALG Helpers:         u=Unchanged y=Enable n=Disable
 -c u|y|n              Content Sharing:         u=unchanged y=Enable n=Disable
 -C u|y|n              Reboot on core dump:     u=unchanged y=Enable n=Disable
 -d u|g|l|<domainname>
    where u            Leave domain name unchanged
          g            Set the domain name to gateway
          l            Set the domain name to lan
          <domainname> Set the domain name to <domainname>
 -e u|y|n              DECT Emission Mode:      u=unchanged y=Enable n=Disable
 -f u|y|n              File Sharing:            u=unchanged y=Enable n=Disable
 -F u|y|n              RTFD root protection:    u=unchanged y=Enable n=Disable
                         NOTE: tch-gui-unhide will ALWAYS enable RTFD protection
 -g u|y|n            * DumaOS (Game Optimiser): u=unchanged y=Enable n=Disable
 -G                    Removes the Guest Wi-Fi SSIDs, firewall rules/zones, and guest networks
 -h u|d|s|<hostname>
    where u            Leave hostname unchanged
          d            Set the hostname to VARIANT (e.g. DJA0231)
          s            Set the hostname to VARIANT-MAC_HEX (e.g. DJA0231-XXXXXX)
          <hostname>   Use the specified hostname
 -i u|y|n              Intercept Daemon:        u=unchanged y=Enable n=Disable
 -I n.n.n.n            Set the LAN IPv4 address to n.n.n.n
 -k a|c|e|k|m|n|x      Override default hardening configuration:
    where a            - Keep Telstra AIR enabled
          c            - Keep CWMP installed
          k            - Keep default public authorized keys
          l            - Keep Telstra APN check
          m            - Keep Telstra monitoring and data collection enabled
          n            - Keep Telstra NTP servers
          q            - Keep Telstra QoS VoWiFi reclassify rules
          x            - Keep noexec on ext2/3/4, fat, hfsplus, hfsplusjournal
                          and ntfs filesystems (i.e. prevent execution of
                          scripts/programs on USB devices)
          T            - Keep all default Telstra configuration (Equivalent
                          to: -ka -kc -kk -kl -km -kn -kq -kx)
                       NOTE: Use case opposite to reverse the override.
                       e.g. -kT -kC keeps all Telstra config except CWMP
 -l u|y|n            * LED logging:             u=unchanged y=Enable n=Disable 
 -m u|a|b|c|v|y|n    * MultiAP (EasyMesh):      u=unchanged a=Enable Agent
                                                            b=Enable BackHaul SSID
                                                            c=Enable Controller 
                                                            v=Enable Vendor Extensions
                                                            y=same as -ma -mb -mc -mv
                                                            n=Disable
 -n u|a|c|g|f|o|<n.n.n.n>
    where u            Leave DNS servers unchanged
          a            Automatically use the DNS servers from the ISP
          c            Set the DNS servers to Cloudflare
          g            Set the DNS servers to Google
          f            Set the DNS servers to OpenDNS Family Shield
          o            Set the DNS servers to OpenDNS
          <n.n.n.n>    Set the DNS servers to 1 or 2 comma-separated
                        IPv4 addresses (e.g. 8.8.8.8,1.1.1.1)
 -o                    Configures opkg
 -O 17|18|19           Overrides the default opkg repository with the specified version
 -p u|y|n|d            Power Saving:            u=unchanged y=Enable n=Disable d=Default
 -q u|y|n            * NFC:                     u=unchanged y=Enable n=Disable
 -r u|y|n              Printer Sharing:         u=unchanged y=Enable n=Disable
 -s u|b|d|n            WAN Supervision:         u=unchanged b=BFD d=DNS n=Disable
 -t u|y|n|m            Telephony:               u=unchanged y=Enable n=Disable
                                                m=Enable w/o services (ex. DND/HOLD/MWI)
 -u u|y|n              UPnP Service:            u=unchanged y=Enable n=Disable
 -w u|y|n              WPS:                     u=unchanged y=Enable n=Disable
                         (on non-Guest and non-Backhaul SSIDs)
 -y                    Bypass the confirmation prompt (answers 'y')
 -A                    Equivalent to: -hd -dg -an -cn -fn -ln -in -rn -sd -un -wn -Fy
 -S                    Equivalent to: -hs -dg -an -cn -fn -ln -in -rn -sd -un -wn -Fy
 -M                    Minimum memory mode: Equivalent to:
                           -an -cn -fn -in -rn -tn -en -un -mn -gn -qn -Fy
                         PLUS stops and disables the associated services
 -R                    Reset to device defaults: Equivalent to:
                         -h mymodem -d modem -na -ay -cy -fy -iy -pd -ry -ty -ey -uy -my -gy -qy -wy -Fn -sb
 -U                    Download the latest version of de-telstra from GitHub
 --save-defaults       Saves the command line options as defaults for future executions
                         When specified, NO changes are applied to the device
 --show-defaults       Shows the settings that would be applied (defaults and over-rides)
                         When specified, NO changes are applied to the device
 --no-defaults         Ignores any saved defaults for this execution
                         --no-defaults must be the FIRST option specified.
 --no-service-restart  Do NOT restart services after applying configuration changes
 --no-password-remind  Do NOT remind to change root password
```
#### Notes
1. The default for all optional parameters is u (unchanged).
2. The options to disable/enable NFC, EasyMesh and DumaOS are only applicable to devices with those services installed.
3. Shortcut options (e.g. *-A*, *-S*, *-M* and *-R*) can have their settings overridden by specifying the required option **AFTER** the shortcut option. For example, the *-A* option disables Content Sharing (*-cn*). However, you can specify *-A -cy* to enable Content Sharing and still apply all the other options implied by *-A*.

Options are processed sequentially, so if an option is specified twice, only the last one
is used. Saved defaults are applied before commmand line options.

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

## log-check
Checks that the logging RAM buffer and disk-based messages log are both being written correctly.
```
Usage: ./log-check [options]

Options:
 -C    Adds or removes the scheduled hourly cron job
```

## mtd-backup
Backs up mtd or ubifs device partitions to an attached USB device or SSHFS attached filesystem. Only unchanged partitions are backed up after the first execution.

USB device targets have priority over SSHFS filesystems. 
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
 -o              Save the changed overlay content into the 
                   VARIANT-SERIAL-VERSION-overlay-files-backup.tgz file
 -s              Skip recalculation of the checksum of the backed-up 
                   partition, and just save the checksum calculated 
                   to determine if the image has changed.
 -0              Skip backup of mtd0 if a backup already exists 
                   (Ignored for UBIFS partitions)
 --no-drop-cache Skips flushing the RAM page cache after backup
 --no-devices    Skips backing up the mtd or ubifs device partitions
                   (Ignored unless -c, -e and/or -o specified)
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

When restoring an overlay backup from another device with same firmware, MAC addresses and serial numbers found in the configuration will be updated to the new devices MAC addresses and serial number.

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
This script can be used to reset the device to factory defaults whilst preserving root access. It can also (optionally) automatically re-apply some important configuration after the reset.

It is an updated and more exhaustive implementation of the commands from https://hack-technicolor.readthedocs.io/en/stable/Upgrade/#preserving-root-access.
```
Usage: ./reset-to-factory-defaults-with-root [options]

Options:
 -b                 Make a full backup of your configuration from /overlay
                      before resetting to factory defaults.
                     (Requires attached USB device).
 -B                 Configure for bridged mode. Implies --no-forwards, 
                      --no-leases and --no-ula. Ignored if --restore-config
                      is specified.
 -c                 Disable CWMP configuration during first boot after reset.
 -C                 Disable reboot on core dump after reset.
 -d                 Add DNS rewrites to disable CWMP firmware downloads from
                      fwstore.bdms.telstra.net
 -D domain          Add DNS rewrites to disable CWMP firmware downloads from
                      the specified domain. May be specified multiple times.
 -e                 Disable any 'noexec' flags on USB mounted filesystems.
 -f filename        Flashes the specified firmware 'filename' before reset and 
                      reboot. If 'filename' ends with .rbi, it will be unpacked 
                      first, either to an attached USB device, or /tmp if no USB 
                      is detected. 
                      - If 'filename' ends in .rbi or .bin, it will be flashed 
                        into the booted bank, unless -s is specified.
                      - If 'filename' ends with .pkgtb, the firmware will be 
                        flashed into the passive bank using sysupgrade (root 
                        access will be preserved) and banks will be switched on 
                        reboot.
 -h d|n|s|hostname  Sets the device hostname, where:
                      d = Set the hostname to VARIANT
                      n = Set the hostname to the current hostname
                      s = Set the hostname to VARIANT-MAC_HEX
                      hostname = Use the specified hostname
 -i                 Keep the existing LAN IP address after reset and reboot.
                      This is the default if --restore-config is specified.
                      By default, also restores port forwards, static leases
                      and the IPv6 ULA and prefix size (unless --no-forwards, 
                      --no-leases or --no-ula are specified).
 -I n.n.n.n|DHCP    Use IP address n.n.n.n OR obtain the IP address from DHCP
                      after reset and reboot
 -k                 Keep existing SSH keys after reset and reboot.
 -l n.n.n.n:port    Configure logging to a remote syslog server on the specified
                      IP address and port. The port is optional and defaults to
                      514 if not specified.
 -m                 Keep existing mobile operators and profiles, and linked 
                      WWAN profile.
                      Ignored if no mobile profiles found.
 -n                 Do NOT reboot.
 -p password        Set the password after reset and reboot. If not specified,
                      it defaults to root.
 -s                 Apply factory reset and acquire root on the passive bank, 
                      rather than the booted bank, and then switch banks after 
                      reboot. Firmware will also be flashed into the passive 
                      bank. This is the default when flashing a .pkgtb firmware 
                      into the passive bank.
 -v                 Show the reset script after it has been written.
 -y                 Bypass confirmation prompt (answers 'y').
 --force-backup     By default, attempting to overwrite a backup created today
                      will cause the script to abort. Specify this option to
                      force a new backup to overwrite the previous backup.
 --no-backup        Ignore the -b option if a backup was already taken today.
 --no-bank-check    Bypass adding the login notification about whether the bank
                      plan is optimal or not.
 --no-keys-check    Bypass check for updated authorized_keys file.
                      By default if -k is specified, and an authorized_keys file
                      exists in the current directory, and the default
                      authorized_keys has not been updated, then the local
                      authorized_keys file will be restored instead of the 
                      current /etc/dropbear/authorized_keys.
 --no-forwards      Bypass restore of port forwards (ignored unless -i is
                      specified).
 --no-leases        Bypass restore of static leases (ignored unless -i is
                      specified).
 --no-rc.local      Bypass restore of /etc/rc.local.
 --no-sms-db        Bypass restore of the SMS message database.
 --no-ula           Bypass restore of the IPv6 ULA and LAN prefix size (ignored 
                      unless -i is specified).
 --save-defaults    Saves the command line options (except -f/-s/-y) as defaults.
                      When specified, NO changes are applied to the device.
 --show-defaults    Shows the settings that would be applied (defaults and over-rides)
                      When specified, NO changes are applied to the device.
 --no-defaults      Ignores any saved defaults for this execution
                      --no-defaults must be the FIRST option specified.
 -U                 Download the latest version of the script from GitHub.
                      Do NOT specify any other parameters or options if doing
                      a version upgrade.
 --restore-config   Runs the restore-config.sh script after reboot if it is found
                      in the USB backups directory. Output will be written to the 
                      system log. --restore-config should be the LAST option
                      specified, and may optionally be followed by the name of
                      the overlay backup file to be restored. Saved defaults are
                      IGNORED when --restore-config is specified.
 --i                Specifies that the IP address configured by the -i or -I options 
                      is also to be applied after the configuration is restored. If
                      not specified, the IP address used will be the one found in the 
                      configuration backup. Ignored unless --restore-config is also 
                      specified.
```

## safe-firmware-upgrade
Applies a new firmware to the device, without losing root access.

It is basically the same as the procedure as described in http://hack-technicolor.rtfd.io/en/stable/Upgrade/#preserving-root-access and http://hack-technicolor.rtfd.io/en/stable/Upgrade/#flashing-firmware but with many additional options.

This script has a dependency on the `reset-to-factory-defaults-with-root` script. If that script does not exist, or is not the latest version, it will be downloaded as needed.
```
Usage: ./safe-firmware-upgrade [options] filename

Where:
 filename           Is the name of the firmware file to be flashed. If the 
                      filename ends with .rbi, it will be unpacked first, 
                      either to an attached USB device, or /tmp if no USB is 
                      detected. 
                      - If 'filename' ends in .rbi or .bin, it will be flashed 
                        into the booted bank, unless -s is specified.
                      - If 'filename' ends with .pkgtb, the firmware will be 
                        flashed into the passive bank using sysupgrade (root 
                        access will be preserved) and banks will be switched on 
                        reboot.

Options:
 -b                 Make a full backup of your configuration from /overlay
                      before resetting to factory defaults.
                     (Requires attached USB device).
 -B                 Configure for bridged mode. Implies --no-forwards, 
                      --no-leases and --no-ula. Ignored if --restore-config
                      is specified.
 -c                 Disable CWMP configuration during first boot after reset.
 -C                 Disable reboot on core dump after reset.
 -d                 Add DNS rewrites to disable CWMP firmware downloads from
                      fwstore.bdms.telstra.net
 -D domain          Add DNS rewrites to disable CWMP firmware downloads from
                      the specified domain. May be specified multiple times.
 -e                 Disable any 'noexec' flags on USB mounted filesystems.
 -h d|n|s|hostname  Sets the device hostname, where:
                      d = Set the hostname to VARIANT
                      n = Set the hostname to the current hostname
                      s = Set the hostname to VARIANT-MAC_HEX
                      hostname = Use the specified hostname
 -i                 Keep the existing LAN IP address after reset and reboot.
                      This is the default if --restore-config is specified.
                      By default, also restores port forwards, static leases
                      and the IPv6 ULA and prefix size (unless --no-forwards, 
                      --no-leases or --no-ula are specified).
 -I n.n.n.n|DHCP    Use IP address n.n.n.n OR obtain the IP address from DHCP
                      after reset and reboot
 -k                 Keep existing SSH keys after reset and reboot.
 -l n.n.n.n:port    Configure logging to a remote syslog server on the specified
                      IP address and port. The port is optional and defaults to
                      514 if not specified.
 -m                 Keep existing mobile operators and profiles, and linked 
                      WWAN profile.
                      Ignored if no mobile profiles found.
 -n                 Do NOT reboot.
 -p password        Set the password after reset and reboot. If not specified,
                      it defaults to root.
 -s                 Apply factory reset and acquire root on the passive bank, 
                      rather than the booted bank, and then switch banks after 
                      reboot. Firmware will also be flashed into the passive 
                      bank. This is the default when flashing a .pkgtb firmware 
                      into the passive bank.
 -v                 Show the reset script after it has been written.
 -y                 Bypass confirmation prompt (answers 'y').
 --force-backup     By default, attempting to overwrite a backup created today
                      will cause the script to abort. Specify this option to
                      force a new backup to overwrite the previous backup.
 --no-backup        Ignore the -b option if a backup was already taken today.
 --no-bank-check    Bypass adding the login notification about whether the bank
                      plan is optimal or not.
 --no-keys-check    Bypass check for updated authorized_keys file.
                      By default if -k is specified, and an authorized_keys file
                      exists in the current directory, and the default
                      authorized_keys has not been updated, then the local
                      authorized_keys file will be restored instead of the 
                      current /etc/dropbear/authorized_keys.
 --no-forwards      Bypass restore of port forwards (ignored unless -i is
                      specified).
 --no-leases        Bypass restore of static leases (ignored unless -i is
                      specified).
 --no-ula           Bypass restore of the IPv6 ULA and LAN prefix size (ignored 
                      unless -i is specified).
 --no-sms-db        Bypass restore of the SMS message database.
 --save-defaults    Saves the command line options (except filename/-s/-y) as 
                      defaults.
                      When specified, NO changes are applied to the device.
 --show-defaults    Shows the settings that would be applied (defaults and over-rides)
                      When specified, NO changes are applied to the device.
 --no-defaults      Ignores any saved defaults for this execution.
                      --no-defaults must be the FIRST option specified.
 -U                 Download the latest version of the script from GitHub.
                      Do NOT specify any other parameters or options if doing
                      a version upgrade.
 --restore-config   Runs the restore-config.sh script after reboot if it is found
                      in the USB backups directory. Output will be written to the 
                      system log. --restore-config should be the LAST option
                      specified before the firmware filename, and may optionally 
                      be followed by the name of the overlay backup file to be 
                      restored. Saved defaults are IGNORED when --restore-config 
                      is specified.
 --i                Specifies that the IP address configured by the -i or -I options 
                      is also to be applied after the configuration is restored. If
                      not specified, the IP address used will be the one found in the 
                      configuration backup. Ignored unless --restore-config is also 
                      specified.
```

## set-optimal-bank-plan
These devices have 2 firmware banks. Only 1 bank is ever active, and the other is dual purpose: 

1. it is a fail safe in case the active bank won't boot, and 
2. it is used by the over-the-air upgrade (OTA) procedure. When a firmware is loaded OTA, it is written into the passive bank, which is then made active. When the device reboots, it will be on the new firmware and booting from the previously passive bank.

The so-called optimal bank plan takes advantage of a bootloader feature that allows the device to boot into a BOOTP environment that will allow you to load a new firmware via TFTP. Firmware loaded in this way is always written into bank 1, whether or not bank 1 is active. If bank 2 is active and bootable, the device will still boot from that bank and ignore the new firmware that has been loaded via TFTP.

So the optimal bank plan has this configuration:

* Bank 1 is empty but marked as the active bank
* Bank 2 contains the bootable operating firmware

What happens when the device boots is that it attempts to boot the active bank (bank 1) but can't because there is no bootable firmware in it. This causes a boot fail and the device then boots the passive bank. Note that this does not change the active bank – the unbootable bank is still marked as active.

What makes it optimal is that if you lose root access to the firmware in bank 2 for any reason, you can always load a rootable firmware into bank 1 via TFTP and it will always boot into that loaded firmware, because bank 1 is still active. You can then easily reacquire root.

The Telstra Smart Modem Gen 3 is slightly different in that it has a different bootloader, and, unlike earlier generations, it will automatically switch to bank 1 after loading a new firmware via TFTP, so this script is _not_ suitable for the Gen 3 device.

This script will copy the booted and rooted firmware into bank 2, _including_ any customisation you have done (such as changing the root password), and then erase bank 1. It will then set bank 1 as active so that it is always tried first during booting. During a normal boot, bank 1 will fail and it will automatically switch and boot from your rooted firmware in bank 2 (without removing the active status from bank 1). It also means that if you need to use TFTP to load a new firmware into bank 1, it will always boot the loaded firmware by default.
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

## unflock
Checks for and optionally removes stale service file locks.
```
Usage: ./unflock [options]

Options:
 -n secs  Sets the number of seconds to determine when a lock is stale.
            Default if not specified is 60 seconds.
            Ignored when adding the scheduled cron job.
 -r       Removes any found stale locks.
 -C       Adds or removes the scheduled cron job
```
Without the `-r` option, `unflock` will simply report any found stale locks.

When run with the -C option (which should be the only option), the scheduled job will be added if it does not already exist, or removed if it does exist in the schedule. By default, the script will run every minute. You can modify the schedule through the Management card in `tch-gui-unhide`, or by directly modifying the /etc/crontab/root file.

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
When run with the -C option (which should be the only option), the scheduled job will be added if it does not already exist, or removed if it does exist in the schedule. By default, the script will run once month on a randomly selected day at a random time between 2:00am and 5:00am. You can modify the schedule through the Management card in `tch-gui-unhide`, or by directly modifying the /etc/crontab/root file.

# How to download and execute these scripts
If you download a tch-gui-unhide release archive, the scripts applicable to that firmware version are included.

You can also download the latest version individually:

**NOTE: Replace `<script>` with the name of the script you wish to download.**

Execute this command on your device via a PuTTY session or equivalent (an active WAN/Internet connection is required):
```
wget --no-check-certificate https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/utilities/<script> 
```

Alternatively, download the script manually and load it up to your device using WinSCP or equivalent. Make sure you do not change the line endings from Unix to Windows!!

After you have the script on your device, you may need to make it executable, which is done with this command (assuming you are in the same directory as the script):
```
chmod +x <script>
```

Then, execute the script (assuming you are in the same directory into which you downloaded or uploaded the script):
```
./<script> <options>
```
