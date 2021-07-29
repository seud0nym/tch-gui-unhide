# Replacing dnsmasq with AdGuard Home on a Telstra Technicolor Gateway Device
The following instructions are based on this post on the OpenWRT forums: https://forum.openwrt.org/t/howto-running-adguard-home-on-openwrt/51678

The instructions have been adapted for use on the Telstra branded Technicolor devices.

**Please read the following notes carefully to decide if this solution is right for you!**

## What It Does
The setup script will install and configure AdGuard Home on your device, and _disable_ dnsmasq. AdGuard Home will then handle DNS resolution and DHCP for your local network.

It will also download and run the [`intercept-dns`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#intercept-dns) script, to configure DNS interception and hijacking.

# Important Considerations
The things you need to be aware of:

## AdGuard Home is incompatible with the overlay file system
AdGuard Home (or rather the database system that it uses) is _incompatible_ with the overlay file system used on these devices (https://github.com/AdguardTeam/AdGuardHome/issues/1188).

The setup script implements 2 different solutions to this issue:
1. The default installation target is a mounted USB device.
   - Obviously, it must be permanently mounted, or you will not have DNS resolution on your network.
2. You can optionally specify the **-i** option to install AdGuard Home on internal storage.
   - The working directory will be set to tmpfs, but by default this means that *ALL* statistics, downloaded block lists and DHCP static and dynamic leases will be **lost** on reboot.
   - To work around this limitation, the script will install rsync to synchronize the AdGuard Home working directory back to permanent storage every minute and on shutdown. On boot, the permanent copy will be synced back to tmpfs before starting AdGuard Home. The worst case scenario should be that you could lose up to 1 minute of logs, leases and statistics.

#### Other Internal Storage Considerations
- By default, this installation option will reduce log retention to 24 hours, to try and reduce the likelihood of running out of space.
- You will need to carefully monitor internal storage to make sure you do not run out of space. You can disable log retention in AdGuard Home if this becomes an issue.

## DHCP Incompatibility with Guest Networks
The default dnsmasq service provides both DNS forwarding and DHCP. It allows different DHCP pools, so that the different interfaces (LAN and Guest networks) can have different IP address ranges.

The AdGuard Home DHCP Server listens on all interfaces (including the Guest networks) but can only serve addresses from a single pool. Therefore, devices connecting to the Guest Wi-Fi networks would be unable to acquire a valid IP address for their subnet.

This script disables dnsmasq to maximize free memory and enables the AdGuard Home DHCP server, therefore you *cannot* run Guest SSIDs.

*If you require Guest Wi-Fi facilities, then you should **NOT** use this script to install AdGuard Home on your device.*

You can use the [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) script with **-G** option to remove the Guest Wi-FI SSIDs, networks and firewall rules and zones, if you are not using Guest Wi-Fi.

## Free RAM Requirements
The forum post starts by saying that devices require 100MB RAM free. A subsequent post indicates around 30MB.

The actual amount will depend on a variety of factors, including the number of block lists defined.

I haven't run it long enough to know if this is indicative over time, so you should monitor your free memory closely and try and shut down unnecessary services if required. The [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) script has a **-M** option to shutdown and disable optional services to try and free up as much memory as possible.

# Installation Pre-requisites
- A working internet connection on your device
- Your device should be configured correctly. The setup script will copy your current DHCP settings into AdGuard Home, and configure DNS to use AdGuard Home.
- A USB stick (unless you specify installation on internal storage)

# Installation
Read the section on "Optional Installation Parameters" before running  one of the following commands.

## Default Installation on USB Device
Insert a USB device and then run the following command on your device:
```
curl -skL https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/adguard/agh-setup | sh -s --
```
When this command successfully completes, AdGuard Home will be installed and running from the USB Device. The web interface will be accessible at http://[router ip address]:8008. The default username is root and the default  password is agh-admin.

## Running AdGuard Home from Internal Storage
Run the following command on your device:
```
curl -skL https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/adguard/agh-setup | sh -s -- -i
```
When this command successfully completes, AdGuard Home will be installed and running from your internal storage. The web interface will be accessible at http://[router ip address]:8008. The default username is root and the default  password is agh-admin.

## Manual Download of Setup Script
If you are uncomfortable running the script without reviewing it first, simply download it and execute it manually:
```
curl -skL -o agh-setup https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/adguard/agh-setup
chmod +x agh-setup
```
You can execute `./agh-setup -?` to see all available installation options (some of which are also described below).

# Optional Installation Parameters
The following optional configuration parameters may be specified **after** the doubles dashes (**--**) in the above commands:
- -u username
  - The AdGuard Home web user name.
  - If not specified, the user name defaults to: root
- -p password
  - The plain text AdGuard Home web password.
  - If not specified, the password defaults to: agh-admin
  - The password will be hashed by calling https://bcrypt.org/api/generate-hash.json
- -h 'hash'
  - The Bcrypt password hash representing the AdGuard Home web password.
  - If not specified, the password defaults to: agh-admin
  - NOTE: The hash value **MUST** be specified within single quotes.
  - You can generate the hash using an online generator, such as:
    - https://bcrypt.org/
    - https://www.appdevtools.com/bcrypt-generator
    - https://wtools.io/bcrypt-generator-online
  - If you supply a password hash, you *must* also use the **-p** option to specify the matching password, so that AdGuard Home can be checked post-installation and any static leases currently defined in dnsmasq can be loaded.
- -x c|g|i|u|x
  - Exclude features:
    - -xc
      - Do not update the System CA certificates
      - Only use this option if the certificates have already been updated.
      - **NOTE:** If certificates are not up to date, AdGuard Home will *FAIL* to download filters
    - -xg
      - Do not get the latest version of AdGuard Home if it has already been downloaded
    - -xi
      - Do not enable DNS hijacking/interception
    - -xu
      - Do not download utility scripts
    - -xy
      - Do not replace an existing AdGuard Home Configuration file 
      - Has no effect if the AdGuardHome.yaml file does not exist
    - -xx
      - Same as **-xc -xg -xi -xu -xy**
- -s
  - Skips the free memory check
  - If there is not enough free memory, the installation will be aborted.
  - Use this option to bypass the check, and install anyway.

# Post-Installation
When the script completes, you will be able to access AdGuard Home in your browser at http://[router ip address]:8008

# Uninstalling
Run the following command on your device:
```
curl -skL https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/adguard/agh-setup | sh -s -- -r
```
Or, if you downloaded the script manually, you can run `./agh-setup -r`.

When the script completes, AdGuard Home will have been removed and dnsmasq re-enabled with default settings.

## Uninstalling Without Removing AdGuard Home
Use the `-u` option instead of `-r` to prevent deleting the AdGuard Home directory. If you then decide to re-install, the you can skip downloading it again by specifying the `-xg` option when running the setup script.

# Other Utilities
These utility scripts will be automatically downloaded to the AdGuardHome directory (unless you specify the **-xu** or **-xx** installation options).

## agh-change-password
Allows you to change the AdGuard Home password and optionally the user name.

Usage: agh-change-password [parameters]

Parameters:
 - -p password
    - The new plain text AdGuard Home web password. The password will be hashed by calling https://bcrypt.org/api/generate-hash.json
    - This option is ignored if -h is specified.
 - -h 'hash'
    - The Bcrypt password hash representing new the AdGuard Home web password.
    - The hash value **MUST** be specified within single quotes.
    - You can generate the hash using an online generator, such as:
      - https://bcrypt.org/
      - https://www.appdevtools.com/bcrypt-generator
      - https://wtools.io/bcrypt-generator-online
 - -n username
    - The new AdGuard Home web user name.
 - -U
    - Download the latest version of agh-change-password from GitHub

NOTE:
- If -U is specified, it must be the only option.
- One of either -p or -h must be specified.

## agh-lists-update
Updates AdGuard Home with the latest blocklists from https://firebog.net/. It also adds the anudeepND Domain whitelist if it has not already been added.

You can configure the update to run automatically every Saturday morning by specifying the -C option.

Usage: agh-lists-update [-p password [-C]]|[-U]

Parameters:
 - -p password
    - The AdGuard Home web password.
    - Required unless -U is specified.
 - -C
    - Adds or removes the scheduled weekly cron job
 - -U
    - Download the latest version of agh-lists-update from GitHub

NOTE:
- If -U is specified, it must be the only option.
- The password option (-p) is *MANDATORY* unless -U is specified, **OR** if you are removing the cron job.
- The username will be determined automatically from the AdGuard Home configuration.

# Other Scripts

These scripts are not automatically downloaded, as they are not applicable to the standard installation.

## agh-clients.lua
This script extracts the hosts and IP addresses known to the router and updates the client list in an AdGuard Home installation. It's primary purpose is to correctly resolve IPv6 addresses to their internal LAN host names.

The script requires 2 parameters for each server to be updated: the address:port of the AdGuard Home server and the AdGuard Home username:password. You can update multiple AdGuardHome servers by repeating the address and credentials for each server 

It is designed to be run as a scheduled cron task every few minutes. For example, the following cron task added to `/etc/crontabs/root` will update 2 different AdGuard Home servers (running independently on IP addresses 192.168.0.123 and 192.168.0.234) every 6 minutes:
```
*/6 * * * * /usr/bin/lua /mnt/usb/USB-A1/agh-clients.lua 192.168.0.123:8008 root:agh-admin 192.168.0.234:8008 root:agh-backup
```
