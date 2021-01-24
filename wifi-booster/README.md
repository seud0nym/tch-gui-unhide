# Using a Telstra Technicolor DJA0230/DJA0231 as a "Wi-Fi Booster"
The Telstra Smart Modems (Gen 1.1 and Gen 2) implement the EasyMesh standard and can act as the base controller for the Telstra Smart Wi-Fi Booster product, to extend Wi-Fi coverage throughout a home. The devices themselves, however, can also act as a "Wi-Fi Booster". Using one of these devices as a "Booster" also has the added advantage of making additional LAN ports available at the site of the "Booster" device.

EasyMesh is implemented via a service called MultiAP. There are 2 components:
- mulitap_controller
    - Enabled on the primary device to manage the remote boosters. 
    - There must be only 1 controller in a network. It is usually the device connected to the internet. Multiple controllers cause various problems, including SSIDs flip-flopping between controllers, or SSIDs not being propogated.
- multiap_agent
    - Enabled on both the "booster" AND primary devices, to handle device registration, device hand-over between agents, etc.

As far as I can determine, using a DJA0230 or DJA0231 as a "Booster" device requires a wired backhaul, either through a direct ethernet connection or via Ethernet-Over-Power adapters. I have not been able to get it to work with a wireless backhaul like the true Booster product does.

These scripts apply different configuration to the "booster" device to provide various implementation scenarios:

**NOTE : It is *strongly* recommended that you remove any configuration changes that you may have made to the device before commencing.**

You should download the latest release of `tch-gui-unhide`, and execute the following scripts:
1. Reset to Factory Defaults
    - To RTFD and preserve the current LAN IP address: `./reset-to-factory-defaults-with-root -i -b`
    - **OR**
    - To RTFD and set a new LAN IP address (e.g. 192.168.0.2): `./reset-to-factory-defaults-with-root -I 192.168.0.2 -b`
2. Harden your device and remove Telstra SSIDs and monitoring
    - `./de-telstra -S`
3. Change the root password
    - `passwd`
4. Apply tch-gui-unhide
    - `./tch-gui-unhide`
5. Apply your selected "booster" configuration script (i.e. `lan-to-lan-booster` or `lan-to-wan-booster`)

## lan-to-lan-booster
This is the simplest implementation, and uses a LAN port on the primary device connected to a LAN port on the "booster" device. However, since it does require a LAN port on the "booster" device, it leaves only 3 LAN ports available for additional wired devices. The WAN port is not utilised in this implementation.

### Functionality
This script will automate the following functions:
- Disable MultiAP Controller
    - There must only be 1 controller on the LAN.
- Optionally set the MultiAP logging level
    - You can increase the logging level to troubleshoot issues.
- Disable 4G Mobile Backup, WAN Sensing and WAN Supervision
    - Not required, as there is no WAN connection to a "booster" device.
- Disable IPv4 and IPV6 DHCP servers
    - Not required, as DHCP should be provided by the primary device (or other DHCP server you have enabled for your LAN). There should never be multiple DHCP servers on a single LAN.
- Disable the intercept daemon
    - The intercept daemon is used for captive portals, initial setup wizards and offline alerts (wansensing). It is capable of DNS spoofing to a dummy IP and L4 port interception (e.g. intercept guest lan users connecting towards wan). If it is not disabled on the "booster" device, it can sometimes intercept DNS resolution and cause all DNS queries to return 198.18.1.1.
- Disable the Backhaul SSID
    - If you do not have a "real" Telstra Smart Wi-Fi Booster, the backhaul SSID is superflous. It is only used for wireless backhaul.
- Configure the LAN interface
    - The LAN interface normally is configured with just an IP address and subnet mask, but when acting as a LAN device, it also requires configuration of the gateway and DNS server.

### Primary Device (Controller) Configuration
On the primary device, you must manually configure the following items:
1. Reserve the static lease for the "Booster" device if the IP address falls within your DHCP range.
2. Disable the Backhaul SSID, unless you have a "real" Telstra Smart Wi-Fi Booster. It is only used for wireless backhaul, and these scripts configure wired backhaul only.

### Notes
1. A reboot is recommended after running the script.
2. You should see the booster device registered on the primary device within 1 minute. 

## lan-to-wan-booster
This is a more complex implementation, and uses a LAN port on the primary device connected to the WAN port of the "booster" device. This allows all 4 LAN ports to be available for additional wired devices. It also creates an additional Wireless SSID, for emergency management of the "booster" device should it become inaccessible via the LAN.

### Functionality
This script will automate the following functions:
- Disable MultiAP Controller
    - There must only be 1 controller on the LAN.
- Optionally set the MultiAP logging level
    - You can increase the logging level to troubleshoot issues.
- Disable 4G Mobile Backup
    - Not required, as there is should be no true WAN connection to a "booster" device.
- Disable WAN Sensing 
    - Not required, because if there are no alternative WAN interfaces in this scenario.
- Disable WAN Supervision
    - WAN supervision checks the WAN interface status. The primary device (acting as the WAN gateway in this configuration) will not respond correctly to the supervision requests.
- Disable the intercept daemon
    - The intercept daemon is used for captive portals, initial setup wizards and offline alerts (wansensing). It is capable of DNS spoofing to a dummy IP and L4 port interception (e.g. intercept guest lan users connecting towards wan). If it is not disabled on the "booster" device, it can sometimes intercept DNS resolution and cause all DNS queries to return 198.18.1.1.
- Disable the firewall
    - The firewall must be configured to accept incoming connections over the WAN.
- Disable the Backhaul SSID
    - If you do not have a "real" Telstra Smart Wi-Fi Booster, the backhaul SSID is superflous. It is only used for wireless backhaul.
- Enable SSH access from the WAN
    - The "WAN" of the "booster" device is the LAN provided by the gateway device. You can therefore access the device for management using the LAN address, rather than needing to connect to the internal LAN address of the "booster" device.
- Configure the LAN interface on a different subnet to the normal LAN provided by the gateway device.
    - Since we are bridging all 4 LAN ports plus the WAN port into the WAN bridge, we need to make sure that the internal LAN of the "booster" device does not conflict with the "real" LAN.
- Creates a new SSID to enable management of the "booster" device should it become inaccessible from the normal LAN provided by the gateway device.
- Configure the WAN interface 
    - The WAN interface needs to be configured to use Ethernet as the connection mode, and can be configured to use DHCP or a static IP address within your normal LAN subnet.
- Reconfigure all LAN ports and wireless devices to operate on a WAN bridge.
- Reconfigure the interfaces used by MultiAP agent
    - Normally MultiAP is configured to operate on the 4 LAN ports and the wireless interfaces. In this scenario, it must operate on the LAN ports, WAN port and the wireless interfaces.

### Primary Device (Controller) Configuration
On the primary device, you must manually configure the following items:
1. You must reserver a static lease for the "Booster" device if you are using DHCP or if the IP address falls within your DHCP range.
2. Disable the Backhaul SSID, unless you have a "real" Telstra Smart Wi-Fi Booster. It is only used for wireless backhaul, and these scripts configure wired backhaul only.

### Notes
1. After running the script, move the cable from the LAN port to the WAN port.
2. A reboot is recommended after running the script.
3. You should see the booster device registered on the primary device within 1 minute. 
4. If you cannot access the device with a cable connected to a LAN or WAN port, connect using Wi-Fi to the management SSID set up by the script. The default password is the same as the default Wi-Fi password for the device. You should then be able to SSH to the LAN IP configured by the script.

# How to download and execute these scripts

**NOTE : Replace `<scriptname>` with the name of the script (i.e. `lan-to-lan-booster` or `lan-to-wan-booster`) that you wish to download.**

Execute this command on your device via a PuTTY session or equivalent (an active WAN/Internet connection is required):
```
wget https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/wifi-booster/<scriptname> 
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
Use `-?` to see all the options available on the script.

