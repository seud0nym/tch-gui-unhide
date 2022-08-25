# LAN/Wi-Fi Speed Testing
Sometimes you want or need to test the performance of your Wi-Fi or your LAN in relation to your router. This script can install the [OpenSpeedTest Server](https://github.com/openspeedtest/Speed-Test) on your Telstra branded Technicolor device.

## Installation Pre-requisites
- A working internet connection on your device
- The OpenSpeedTest Server requires about 35Mb of storage. If your device does not have sufficient internal storage, you will need to permanently mount a USB stick on the device.

## Installation
Run the following command on your device:
```
curl -skL https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/speedtest/ost-setup | sh -s --
```

The script will check free space requirements and then install if sufficient space is available.

### Manual Download of Setup Script
If you are uncomfortable running the script without reviewing it first, simply download it and execute it manually:
```
curl -skLO https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/speedtest/ost-setup
chmod +x ost-setup
```
Then execute `./ost-setup` to run the installation.

## Post-Installation
When the script completes, you will be able to access OpenSpeedTest in your browser at http://[router ip address]:5678

## Uninstalling
Run the following command on your device:
```
curl -skL https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/speedtest/ost-setup | sh -s -- -r
```
Or, if you downloaded the script manually, you can run `./ost-setup -r`.

When the script completes, OpenSpeedTest will have been removed.

# Internet Speed Testing
If you want to test your internet speed from your router, download the [Ookla SpeedTest command line interface](https://www.speedtest.net/apps/cli).