# Using a Telstra Technicolor DJA0230/DJA0231 as a "Wi-Fi Booster"
The Telstra Smart Modems (Gen 1.1 and Gen 2) implement the EasyMesh standard and can act as the base controller for the Telstra Smart Wi-Fi Booster product, to extend Wi-Fi coverage throughout a home. The devices themselves, however, can also act as a "Wi-Fi Booster". Using one of these devices as a "Booster" also has the added advantage of making additional LAN ports available at the site of the "Booster" device.

EasyMesh is implemented via a service called MultiAP. There are 2 components:
- mulitap_controller
    - Enabled on the primary device to manage the remote boosters. 
    - There must be only 1 controller in a network. It is usually the device connected to the internet. Multiple controllers cause various problems, including SSIDs flip-flopping between controllers, or SSIDs not being propagated.
- multiap_agent
    - Enabled on both the "booster" AND primary devices, to handle device registration, device hand-over between agents, etc.

As far as I can determine, using a DJA0230 or DJA0231 as a "Booster" device requires a wired back-haul, either through a direct ethernet connection or via Ethernet-Over-Power adapters. I have not been able to get it to work with a wireless back-haul like the true Booster product does.

**NOTE : It is *strongly* recommended that you remove any configuration changes that you may have made to the device before commencing.**

You should download the latest release of `tch-gui-unhide`, and execute the following scripts:
1. Reset to Factory Defaults
    - To RTFD and preserve the current LAN IP address: `./reset-to-factory-defaults-with-root -i -c -b`
    - **OR**
    - To RTFD and set a new LAN IP address (e.g. 192.168.0.2): `./reset-to-factory-defaults-with-root -I 192.168.0.2 -c -b`
2. Harden your device, shutdown unnecessary services, and remove Telstra SSIDs and monitoring e.g.
    - `./de-telstra -S -tn -en`
3. Change the root password
    - `passwd`
4. Apply tch-gui-unhide e.g.
    - `./tch-gui-unhide -hs -y`
5. Download and apply the `bridged-booster` configuration script

### Previous Configuration Script Versions
Previously there were two different scripts (`lan-to-lan-booster` and `lan-to-wan-booster`) that applied different configuration to the "booster" device to provide various implementation scenarios. These have been replaced by a single script (`bridged-booster`) that is simpler and hopefully more robust, whilst providing maximum functionality.

#### Migration from Previous Scripts
You should be able to migrate to the latest configuration by simply downloading and running the `bridged-booster` script. My "booster" device was originally configured with the `lan-to-wan-booster` script, and is now working as expected after running the new `bridged-booster` script *without* first resetting the device to factory defaults.

If you experience any issues, then follow the complete instructions above to do a factory reset and apply the new script.

## bridged-booster
This script sets up the "booster" device in bridged mode. This configures *all* ethernet ports as LAN ports, which means any port can be used to connect to the primary device, and the other four ports are available for additional wired devices.

### Functionality
This script will automate the following functions:
- Sets the device to Bridged Mode
    - This removes the WAN interfaces and makes all the ethernet ports LAN ports, thereby giving you the ability to plug in up to four wired devices into the "booster" device.
- Disables the MultiAP Controller
    - There must only be 1 controller on the LAN.
- Optionally set the MultiAP logging level
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

### Usage
```
Usage: ./bridged-booster [options]

Options:
 -i n.n.n.n  Set LAN IP address to n.n.n.n (default is the current LAN IP address if it is in the 192.168.0.0/24 range, or 192.168.0.2 if it is not)
              The IP address must be in the same LAN subnet as the primary device
 -m n.n.n.n  Set LAN Subnet Mask to n.n.n.n (default is 255.255.255.0)
              The Subnet Mask must be the same as the LAN subnet mask configured on the primary device
 -g n.n.n.n  Set LAN Gateway IP address to n.n.n.n (default is 192.168.0.1)
              This is the LAN IP address of the primary device
 -n n.n.n.n  Set LAN DNS Server IP address to n.n.n.n 
              Specify multiple times for multiple DNS servers. Default is teh LAN Gateway IP address.
 -l 0-9      Set MultiAP logging level 
              0=off 2=Default 9=very verbose
 -r          Skip reboot (NOT recommended!)
 -y          Skip the confirmation prompt
```

### Primary Device (Controller) Configuration
On the primary device, you must manually configure the following items:
1. You must reserve a static lease for the "Booster" device if the IP address falls within your DHCP range.
2. Disable the Back-haul SSID, unless you have a "real" Telstra Smart Wi-Fi Booster. It is only used for wireless back-haul, and these scripts configure wired back-haul only.

### Notes
1. After running the script, you can move the cable from the LAN port to the WAN port if you wish. This is not mandatory, as when the device is running in bridged mode, all 5 ports are effectively LAN ports.
2. A reboot is recommended after running the script. (The script will automatically reboot the device unless you specify otherwise.)
3. You should see the booster device registered on the primary device within 1 minute. 

# How to download and execute the script
Execute this command on your device via a PuTTY session or equivalent (an active WAN/Internet connection is required):
```
wget https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/wifi-booster/bridged-booster 
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

