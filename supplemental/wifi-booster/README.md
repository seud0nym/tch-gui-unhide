# Using a Telstra Technicolor Smart Modem as a "Wi-Fi Booster"
EasyMesh has two components:
- Controller
    - Enabled on the primary device to manage the remote boosters. 
    - There must be only 1 controller in a network. It is usually the device connected to the internet. Multiple controllers cause various problems, including SSIDs flip-flopping between controllers, or SSIDs not being propagated.
- Agent
    - Enabled on both the "booster" AND primary devices, to handle device registration, device hand-over between agents, etc.

The Telstra Smart Modems (Gen 1.1 DJA0230 and Gen 2 DJA0231) implement the EasyMesh standard and can act as the base controller for the Telstra Smart Wi-Fi Booster product, to extend Wi-Fi coverage throughout a home. The devices themselves, however, can also act as a "Wi-Fi Booster". Using these devices as a "Booster" also has the added advantage of making additional LAN ports available at the site of the "Booster" device.

The Telstra Smart Modem Gen 3 (CobraXh) also implements the EasyMesh standard, but has a different implementation to the earlier generation devices.

## Back-Haul

Using a Smart modem as a "Booster" device requires a wired back-haul, either through a direct ethernet connection or via Ethernet-Over-Power adapters. Wireless backhaul is not possible at this time.

## Device Compatibility Matrix

|                                 | DJA0230 (Gen 1)<sup>1</sup> | DJA0231 (Gen 2)<sup>1</sup> | CobraXh (Gen 3)<sup>2</sup> |
|---------------------------------|-----------------------------|-----------------------------|-----------------------------|
| **DJA0230 (Gen 1)**<sup>1</sup> | Controller or Booster       | Controller or Booster       | N/A                         |
| **DJA0231 (Gen 2)**<sup>1</sup> | Controller or Booster       | Controller or Booster       | N/A                         |
| **CobraXh (Gen 3)**<sup>2</sup> | N/A                         | N/A                         | Controller or Booster       | 

<sup>1</sup> Firmware 20.3.c only  
<sup>2</sup> Firmware 21.4 only

You can mix Gen 1.1 (DJA0230) and Gen 2 (DJA0231) devices, either as controller or booster. Gen 3 (CobraXh) devices _cannot_ be used as either a controller or booster in conjunction with any _previous_ generation (Gen 1 or Gen 2) device. However, if the Gen 3 devices are running firmware 21.4.0439 or later, they _can_ be used in a controller or booster configuration with another Gen 3 device. Gen 3 devices running firmware 20.4 cannot be used as a controller or booster.

## Installation

**NOTE : It is *strongly* recommended that you remove any configuration changes that you may have made to the device before commencing.**

You should download the latest release of `tch-gui-unhide`, and execute the following scripts:
1. Reset to Factory Defaults
    - `./reset-to-factory-defaults-with-root`
2. Harden your device, shutdown unnecessary services, remove Guest and Telstra SSIDs, and Telstra monitoring e.g.
    - `./de-telstra -S -M -G -ma`
    - Use `./de-telstra -?` to see all options.
3. Change the root password
    - `passwd`
4. Apply tch-gui-unhide e.g.
    - `./tch-gui-unhide -hs -y`
    - Use `./tch-gui-unhide -?` to see all options.
5. Download and apply the `bridged-booster` configuration script

## bridged-booster Script
This script sets up the "booster" device in bridged mode. This configures *all* ethernet ports as LAN ports, which means any port can be used to connect to the primary device, and the other four ports are available for additional wired devices.

### Functionality
This script will automate the following functions:
- Sets the device to Bridged Mode
    - This removes the WAN interfaces and makes all the ethernet ports LAN ports, thereby giving you the ability to plug in up to four wired devices into the "booster" device.
- Disables the EasyMesh Controller
    - There must only be 1 controller on the LAN.
- Optionally set the EasyMesh logging level (only applicable to Gen 1.1 and Gen 2 devices)
    - You can increase the logging level to troubleshoot issues.
- Disables WAN Sensing 
    - Not required, because if there are no alternative WAN interfaces in this scenario.
- Disables WAN Supervision
    - WAN supervision checks the WAN interface status. The primary device (acting as the WAN gateway in this configuration) will not respond correctly to the supervision requests.
- Disables the 4G Mobile Backup
    - Not required, as there is no WAN connection in a bridged device.
- Disables the IPv4 and IPv6 DHCP servers
    - Not required, as there should only be one DHCP server on the network.
- Disables the intercept daemon
    - The intercept daemon is used for captive portals, initial setup wizards and offline alerts (wansensing). It is capable of DNS spoofing to a dummy IP and L4 port interception (e.g. intercept guest lan users connecting towards wan). If it is not disabled on the "booster" device, it can sometimes intercept DNS resolution and cause all DNS queries to return 198.18.1.1.
- Disables the Back-haul SSID
    - If you do not have a "real" Telstra Smart Wi-Fi Booster, the back-haul SSID is superfluous. It is only used for wireless back-haul.
- Disables QoS
    - Not required as there will be no WAN interface.
- Disables the DumaOS web servers
    - To free up some memory

The script can also restore your device from Bridged Mode to Routed Mode (with the -R option).

### Usage
```
Usage: ./bridged-booster [options]

Options:
 -d          Set LAN IP address, Subnet Mask and Gateway using DHCP
              (This option is ignored if -R specified)
 -i n.n.n.n  Set LAN IP address to n.n.n.n (default is -i192.168.0.2)
              The IP address must be in the same LAN subnet as the primary device
              (This option is ignored if -d specified)
 -m n.n.n.n  Set LAN Subnet Mask to n.n.n.n (default is -m255.255.255.0)
              The Subnet Mask must be the same as the LAN subnet mask configured 
              on the primary device.
              (This option is ignored if -d specified)
 -g n.n.n.n  Set LAN Gateway IP address to n.n.n.n (default is -g192.168.0.1)
              This is the LAN IP address of the primary device.
              (This option is ignored if -d or -R specified)
 -n n.n.n.n  Set LAN DNS Server IP address to n.n.n.n
              Specify multiple times for multiple DNS servers. 
              Default is the LAN Gateway IP address unless -d is specified, in
              which case the DNS Servers will be obtained via DHCP.
              (This option is ignored if -R specified)
 -6          Enable LAN IPv6
              (This option is ignored if -R specified)
 -l 0-9      Set EasyMesh logging level (only applicable to Gen 1.1 and Gen 2 devices)
              0=off 2=Default 9=very verbose
 -R          Restore to Routed Mode
 -r          Skip reboot (NOT recommended!)
 -y          Skip the confirmation prompt
```

### Primary Device (Controller) Configuration
On the primary device, you must manually configure the following items:
1. You must reserve a static lease for the "Booster" device if the IP address falls within your DHCP range.
2. Disable the Back-haul SSID, unless you have a "real" Telstra Smart Wi-Fi Booster. It is only used for wireless back-haul, and these scripts configure wired back-haul only.

### Booster Device (Agent) SSIDs Not Updated
If the booster device does not acquire either or both of the SSIDs, disable and then re-enable the Agent on the booster device. This normally resolves the issue within a minute.

### Notes
1. After running the script, you can move the cable from the LAN port to the WAN port if you wish. This is not mandatory, as when the device is running in bridged mode, all 5 ports are effectively LAN ports.
2. A reboot is recommended after running the script. (The script will automatically reboot the device unless you specify otherwise.)
3. You should see the booster device registered on the primary device within 1 minute. 
4. If the booster device does not report that 3 SSIDs have been synced and/or the controller doesn't recognise the booster, try disabling the Agent on the booster device, save, wait a minute or two, then re-enable.

# How to download and execute the script
Execute this command on your device via a PuTTY session or equivalent (an active WAN/Internet connection is required):
```
curl -kLO https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/supplemental/wifi-booster/bridged-booster
```

Alternatively, download the script manually and load it up to your device using WinSCP or equivalent.

After you have the script on your device, you may need to make it executable, which is done with this command (assuming you are in the same directory as the script):
```
chmod +x bridged-booster
```

Then, execute the script (assuming you are in the same directory into which you downloaded or uploaded the script):
```
./bridged-booster <options>
```
Use `-?` to see all the options available on the script.

