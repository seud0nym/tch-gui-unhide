# Configuration Restore

These scripts are auto-downloaded by the `restore-config.sh` script, which is automatically created in the backups directory by an execution of the [`mtd-backup`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#mtd-backup) script when both the `-c` and `-o` parameters are specified.

This goes beyond the simple "dump-and-load" approach of the built-in configuration export and import. It will re-install missing packages and automatically re-run the [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) and [`tch-gui-unhide`](https://github.com/seud0nym/tch-gui-unhide#readme) scripts with their original parameters. 

It can be used to:
- upgrade firmware without tediously re-entering all your configuration; and
- restore configuration to a replacement device (even to a different Technicolor variant and firmware version) should a primary device fail.

## restore-config.sh Options
```
Usage: restore-config.sh [options] <overlay-files-backup>

Where:
  <overlay-files-backup>  
      is the name of the overlay files backup tgz file from which the 
      configuration will be restored. 
        e.g. DJA0231-CP1234S6789-20.3.c.0329-overlay-files-backup.tgz
      If not specified, it will attempt to find a backup by matching
      on device variant, serial number and optionally firmware version.

Options:
  -n  Do NOT reboot after restoring the configuration.
        This is NOT recommended.
  -t  Enable test mode. In test mode:
        - Wi-Fi will be disabled;
        - Dynamic DNS will be disabled;
        - Telephony will be disabled; and
        - "-TEST" is appended to the hostname and browser tabs if the 
            serial number in the backup does not match the device.
  -v  Enable debug messages. 
        Use twice for verbose messages.
  -y  Bypass confirmation prompt (answers 'y')
```

**NOTE:** You do not need to have downloaded [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) and/or [`tch-gui-unhide`](https://github.com/seud0nym/tch-gui-unhide#readme); the restore will either locate these scripts or download them as needed.

## Examples

### Firmware Upgrade

1. Download the up-to-date versions of [`mtd-backup`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#mtd-backup) and [`reset-to-factory-defaults-with-root`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#reset-to-factory-defaults-with-root):
      - `./mtd-backup -U`
      - `./reset-to-factory-defaults-with-root -U`
2. Insert a USB stick into the device.
3. Backup the MTD partitions, the overlay content and the current configuration:
      - `./mtd-backup -co0lvy`
4. Download the firmware .rbi file to the USB stick.
5. Upgrade the firmware and restore the current configuration:
      - `./reset-to-factory-defaults-with-root --restore-config`
6. The device will reboot *twice* and then you should be on the new firmware with your restored configuration.

If there are any issues with your updated firmware and configuration, you can revert to the previous version with `./mtd-restore -fsr`.

### Restore Configuration to New Device After Failure

(*This assumes you have scheduled daily backups of your device to USB by running* `./mtd-backup -C` *to create the cron task, and that the new device has been factory reset and root access acquired.*)

1. Remove the USB stick from the dead device and insert it into the replacement device.
2. Restore the original configuration:
      - `cd /mnt/usb/A1/backups`
      - `./restore-configuration.sh `*`original_variant`*`-`*`original_serial`*`-`*`original_firmware`*`-overlay-files-backup.tgz`
3. Once the device reboots, your configuration should have been restored.

### Testing

(*This assumes your test device has been factory reset and root access acquired.*)

1. Remove the USB stick from the original device and insert it into the test device.
2. Restore the original configuration in test mode:
      - `cd /mnt/usb/A1/backups`
      - `./restore-configuration.sh -vvt `*`original_variant`*`-`*`original_serial`*`-`*`original_firmware`*`-overlay-files-backup.tgz`
3. Once the device reboots, your original configuration should have been restored to the test device.
      - The IP address of the test device will be unchanged.
      - The hostname and browser tabs will have `-TEST` appended to easily identify the test device.
      - Wi-Fi will be disabled (because the SSIDs are copied from the original device and may impact day-to-day use if left enabled)
      - Telephony will be disabled (because you can't have 2 devices registering for the same number)
      - Dynamic DDNS will be disabled (because it is a test)

## Extensibility

The extension scripts in this folder restore the major configuration elements that can be configured through a device with both [`de-telstra`](https://github.com/seud0nym/tch-gui-unhide/tree/master/utilities#de-telstra) and [`tch-gui-unhide`](https://github.com/seud0nym/tch-gui-unhide#readme) applied.

However, these scripts may not restore some other custom configuration that you have applied to your device.

To allow your custom configuration to be included in the restore process, you can create your own extension configuration restore script and place it in the `backups\restore-config` directory on your USB device.

### Script Naming Rules

- All script names are prefixed by a 3-digit decimal number, followed by a dash (`-`) then a name describing their function and ending with the `.sh` suffix.
- Script numbers in the range 000-099 are reserved for configuring the restore environment and executing the extension scripts.
- Do not duplicate an existing number. There are plenty of gaps between existing scripts.

Scripts are executed in sequence using their prefix number. Carefully consider the order in which your additional scripts should be executed.

### Script Environment

The extension scripts have access to the following environment when they execute:

#### Variables

| Name | Description |
| --- | --- |
| BACKUP_DIR | The directory path containing the backup files generated by `mtd-backup` |
| BACKUP_SERIAL | The serial number of the device from which the backup was taken |
| BACKUP_VARIANT | The device variant (e.g. DJA0230 or DJA0231) from which the backup was taken |
| BACKUP_VERSION | The firmware version of the device from which the backup was taken |
| BANK2 | The directory path where the overlay backup is partially restored |
| BASE | The temporary work directory for the restore |
| CONFIG | The path to the device configuration backup (generated by `mtd-backup -c`) from which configuration is being restored |
| DEBUG | A `y` or `n` flag indicating if debugging is enabled |
| DEVICE_SERIAL | The serial number of the device to which the backup is being restored |
| DEVICE_VARIANT | The device variant (e.g. DJA0230 or DJA0231) to which the backup is being restored |
| DEVICE_VERSION | The firmware version of the device to which the backup is being restored |
| OVERLAY | The path to the overlay backup (generated by `mtd-backup -o`) from which configuration is being restored |
| REBOOT | A `y` or `n` flag indicating if the device is to be rebooted on completion |
| TEST_MODE | A `y` or `n` flag indicating if the restore is running in test mode |
| UCI | The `uci` command to get or show configuration from the **backup** device | 
| VERBOSE | A `y` or `n` flag indicating if verbose debugging is enabled |

#### Functions

| Name | Parameters | Description |
| --- | --- | --- |
| log | level message | `level`: One of **D** (Debug), **V** (Verbose), **I** (Info), **W** (Warn), **E** (Error)<br/>`message`: The message to be logged. |
| download | url directory | `url`: The URL to download<br/>`directory`: The directory into which the file will be saved. It will be named the same as the file name in the URL. |
| restore_file | path <...>  | `path`: The full path and filename to be restored (e.g. `/etc/config/network`). Multiple paths may be specified. If the file is marked as deleted in the backup, it will be **deleted** from the target device. |
| restore_directory | directory | `directory`: The name of the directory to be restored. Files will be copied from the backup or deleted if marked so in the backup. |
| uci_copy | config type exclude lists | `config`: The config file name (e.g. `'web'`).<br/>`type`: The config section type (e.g. `'rule'`)<br/>`exclude`: A regular expression that will be applied against the entire path returned by `$UCI show` and if a match is found, the section will be **excluded**. (e.g. `'dumaos\|homepage'`)<br/>`lists`: A space delimited list of configuration items that are lists, not single values. (e.g. `'roles'`) |
| uci_copy_by_match | config type match option <...> | `config`: The config file name (e.g. `'firewall'`).<br/>`type`: The config section type (e.g. `'include'`)<br/>`match`: The option on which to match the configuration section between teh backup and target device. (e.g. `'path'`)<br>`option`: One or more space-separated names to be copied from the backup configuration. (e.g `'enabled' 'reload'`) |
| uci_set | expr delete_if_empty islist | `expr`: *Either* a path to be copied from the backup to the target device (e.g. `uci_set wansensing.global.enable`) *or* a full path=value expression (e.g. `uci_set multiap.agent.macaddress="$agent_mac"`).<br/>`delete_if_empty`: Specify `y` if the target option is to be deleted if no value is found in the backup configuration.<br/>`islist`: Specify `y` if the target option is a list to which the value is to be added. |

### Default Extension Scripts
| Script | Description |
| --- | --- |
| 000-core.sh | The main system script that sets up the environment |
| 099-extensions.sh | The main system script that runs the other extensions scripts |
| 100-root.sh | Restores the /root directory, including the traffic monitoring stats and history |
| 150-system.sh | Partially restores the system configuration in /etc/config/system |
| 200-authentication.sh | Restores password files and SSH keys | 
| 250-wan_services_config.sh | Restores /etc/config/ddns, /etc/config/iperf and /etc/config/wol |
| 300-power.sh | Restores /etc/config/power between compatible firmware versions |
| 350-network.sh | Restores /etc/config/dhcp, /etc/config/network, /etc/config/wireless, /etc/config/user_friendly_name, and partially restores /etc/config/wansensing | 
| 400-multiap.sh | Restores /etc/config/multiap if it is supported on both the backup and target device |
| 450-qos.sh | Restores /etc/config/bqos (tch-gui-unhide buffer bloat and MAC shaping) and any defined egress shapers |
| 500-telephony.sh | Restores the mmpbx config files, fixes the user agent and applies the SIP passwords |
| 550-cron.sh | Restores scheduled cron tasks |
| 600-sharing.sh | Restores the configuration for file, content and printer sharing |
| 650-de_telstra.sh | Calculates the de-telstra options applied on the backup device and runs de-telstra with those options |
| 700-packages.sh | Removes and installs packages and restores /etc/config/adblock /etc/config/minidlna and /etc/rsyncd.conf |
| 750-parental.sh | Restores /etc/config/parental |
| 800-tod.sh | Restores /etc/config/tod |
| 850-tch_gui_unhide.sh | Restores GUI files and configuration (including web password) and runs tch-gui-unhide with the correct options |
| 900-firewall.sh | Restores the firewall config and defaults, system rules (by matching on rule name), includes (by matching on path), user rules, port forwarding rules, DNS Interception, DMZ and DoS protect configuration |
| 950-service_status.sh | Disables any services that were disabled on the backup device |
