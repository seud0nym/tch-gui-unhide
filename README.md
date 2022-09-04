[![License](https://img.shields.io/github/license/seud0nym/tch-gui-unhide.svg?style=flat)](https://github.com/seud0nym/tch-gui-unhide/blob/master/LICENSE) 
![Languages](https://img.shields.io/github/languages/count/seud0nym/tch-gui-unhide)
![Top Language](https://img.shields.io/github/languages/top/seud0nym/tch-gui-unhide)
![Total Downloads](https://img.shields.io/github/downloads/seud0nym/tch-gui-unhide/total)
[![Latest Release](https://img.shields.io/github/release/seud0nym/tch-gui-unhide/all.svg?style=flat&label=latest)](https://github.com/seud0nym/tch-gui-unhide/releases) 
![Latest Release Downloads](https://img.shields.io/github/downloads/seud0nym/tch-gui-unhide/latest/total)
# Unlock the GUI on your Telstra Technicolor Device
These scripts can be applied to various Telstra branded Technicolor devices to unlock hidden features in the web GUI.

[Features](https://github.com/seud0nym/tch-gui-unhide/wiki/Features) that have been unlocked or added are shown in the [wiki](https://github.com/seud0nym/tch-gui-unhide/wiki).

[Installation instructions](https://github.com/seud0nym/tch-gui-unhide/wiki/Installation) can also be found in the [wiki](https://github.com/seud0nym/tch-gui-unhide/wiki).

![Night Theme with Monochrome Highlights](https://github.com/seud0nym/tch-gui-unhide/wiki/images/night-mono.png)

# Additional Feature Configuration

Using scripts found in this repository, you can configure your device with additional features:

## [AdGuard Home](https://github.com/seud0nym/tch-gui-unhide/tree/master/adguard#readme)

Allows you to install and configure [AdGuard Home](https://github.com/AdguardTeam/AdGuardHome) on your device, and _disable_ dnsmasq. AdGuard Home will then handle DNS resolution and DHCP for your local network.

## [OpenSpeedTest](https://github.com/seud0nym/tch-gui-unhide/tree/master/speedtest#readme)

Allows you to install the [OpenSpeedTest Server](https://github.com/openspeedtest/Speed-Test) on your device, to test the performance of your Wi-Fi or your LAN in relation to your router.

## [Wi-Fi Booster](https://github.com/seud0nym/tch-gui-unhide/tree/master/wifi-booster#readme)

Allows you to configure an EasyMesh-capable Telstra Smart Modem as a "Wi-Fi Booster" to extend Wi-Fi coverage throughout a home (wired back-haul only).

# Utility Scripts

This repository (and the releases) also contain a number of utility scripts to automate many of the functions required to set up your device correctly.

| Script | Function |
| ------ | -------- |
| [de-telstra](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#hijack-dns) | Device hardening, and removal of Telstra monitoring/data collection |
| [dumaos](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#dumaos) | Enables or disables DumaOS (if device is capable) |
| [hijack-dns](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#hijack-dns) | Hijacks DNS requests to ensure that they are handled by the device, or by a specified DNS Server |
| [mtd-backup](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#mtd-backup) | Backs up mtd or ubifs device partitions to an attached USB device or SSHFS attached filesystem |
| [mtd-restore](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#mtd-restore) | Restores mtd or ubifs partitions from an attached USB device or SSHFS filesystem |
| [overlay-restore](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#overlay-restore) | Restores an overlay tar backup |
| [reboot-on-coredump](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#reboot-on-coredump) | Enables or disables reboot on core dump |
| [reset-to-factory-defaults-with-root](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#reset-to-factory-defaults-with-root) | Reset the device to factory defaults whilst preserving root access |
| [safe-firmware-upgrade](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#safe-firmware-upgrade) | Applies a new firmware to the device, without losing root access |
| [set-optimal-bank-plan](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#set-optimal-bank-plan) | Applies the optimal bank plan to the device |
| [set-web-admin-password](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#set-web-admin-password) | Allows you to set or remove the web admin password |
| [show-bank-plan](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#show-bank-plan) | Shows the bank plan, with a final analysis to show whether the bank plan is optimal, or not |
| [unpack-rbi](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#unpack-rbi) | Unpacks the *.rbi* file passed as the first parameter |
| [update-ca-certificates](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#update-ca-certificates) | Downloads and installs the latest version of the System CA Certificates | 

---

# Thanks

This would not have been possible without the following resources:
- https://hack-technicolor.rtfd.io
- https://forums.whirlpool.net.au/thread/9vxxl849
- https://github.com/Ansuel/tch-nginx-gui

Thank you to you all.
