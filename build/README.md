# Development

A quick guide to developing `tch-gui-unhide`...

## Environment

Development is currently done in [Visual Studio Code](https://code.visualstudio.com/) using the following extensions:

- [Visual Studio Code WSL](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-wsl) by Microsoft
- [Lua Language Server](https://marketplace.visualstudio.com/items?itemName=sumneko.lua) by sumneko for Lua development
- [Code Spell Checker](https://marketplace.visualstudio.com/items?itemName=streetsidesoftware.code-spell-checker) by Street Side Software

All development is done in WSL to build the Linux deployment scripts and tar archives necessary for running on the target devices, which are of course also Linux based.

## Code Overview

`tch-gui-unhide` consists of 2 distinct code lines:

1. Shell script snippets that apply modifications to existing Technicolor code; and
2. Lua code to completely replace existing Technicolor code or to add new features.

### Shell Script Snippets

Rather than dealing with one enormous script, the `tch-gui-unhide` installer script is made up of many individual shell script snippets that are combined by the [build](https://github.com/seud0nym/tch-gui-unhide/blob/master/build/build) script to create the installer script for each target firmware version. There are a set of [common](https://github.com/seud0nym/tch-gui-unhide/tree/master/build/common) script snippets that are applied to all firmware versions, and there are separate snippets that are applied only to a specific firmware version. 

The combination of common and firmware-specific script snippets make up a unique installer to be applied to a specific firmware version.

The Technicolor devices use OpenWrt, and its standard unix shell is the Busybox-fork of the Debian implementation of the Almquist shell[<sup>1</sup>](https://openwrt.org/docs/guide-user/base-system/user.beginner.cli#command-line_interpreter) (`ash`). All snippets should assume only basic Bourne shell functionality (i.e. do *not* use `bash` syntax!).

### Lua code

The Technicolor use a proprietary Lua framework for displaying the web user interface, delivered via [nginx](https://nginx.org/en/) (various versions up to 1.12) using [OpenResty](https://openresty-reference.readthedocs.io/en/latest/) (version unknown) on Lua (version 5.1)

The Lua code consists of three main components:

#### Cards

Cards are the individual tiles (cards) displayed on the advanced dashboard.

#### Modals

Modals are the pop-up screens displayed when the card is clicked

#### Transformer Mappings

The nginx worker process runs as the `nobody` user, and therefore has no access to anything that requires root access. The transformer service and its associated mappings allows the Lua code running in nginx to make privileged system calls.

## Directory Structure

### Build Scripts

The [build](https://github.com/seud0nym/tch-gui-unhide/tree/master/build) directory contains the following directories:

- [common](https://github.com/seud0nym/tch-gui-unhide/tree/master/build/common)
    - This directory contains the shell script snippets that are common to all firmware versions.
    - The file [055-Additional](https://github.com/seud0nym/tch-gui-unhide/blob/master/build/common/055-Additional) is a special file that does not contain shell code. This file lists the directories under the [src/common](https://github.com/seud0nym/tch-gui-unhide/tree/master/src/common) directory that are to be included for all firmware targets.
- [17.2](https://github.com/seud0nym/tch-gui-unhide/tree/master/build/17.2), [18.1.c](https://github.com/seud0nym/tch-gui-unhide/tree/master/build/18.1.c), [20.3.c](https://github.com/seud0nym/tch-gui-unhide/tree/master/build/20.3.c) and [20.4](https://github.com/seud0nym/tch-gui-unhide/tree/master/build/20.4)
    - These directories contain the shell script snippets that are specific to a firmware version.
    - Each of these directories may also contain a `055-Additional` file that lists the directories under the [src/common](https://github.com/seud0nym/tch-gui-unhide/tree/master/src/common) directory that are to be included for the specific firmware target.

### Source Code

The [src](https://github.com/seud0nym/tch-gui-unhide/tree/master/src) directory contains files that will replace existing files or be added to the target firmware. The majority of the files contain Lua code, but there are some other file types like configuration files included in the structure. The directories immediately below the lua directory are:
- [common](https://github.com/seud0nym/tch-gui-unhide/tree/master/src/common)
    - This directory contains the code that is common to all firmware versions.
    - Code is separated into directories that deal with a single feature (usually related to a specific card).
- [17.2](https://github.com/seud0nym/tch-gui-unhide/tree/master/src/17.2), [18.1.c](https://github.com/seud0nym/tch-gui-unhide/tree/master/src/18.1.c), [20.3.c](https://github.com/seud0nym/tch-gui-unhide/tree/master/src/20.3.c) and [20.4](https://github.com/seud0nym/tch-gui-unhide/tree/master/src/20.4)
    - These directories contain the code that is specific to a firmware version.
    - The directories can optionally contain either or both of the following build control files:
        - `.exclude`
            - Contains a list of _absolute_ file paths (in relation to the target firmware) that are to be _excluded_ when building the installer for this firmware version.
        - `.include`
            - Contains a list of files from other firmware-specific directories that are to be _included_ when building the installer for this firmware version.
            - Lines in this file must be specified in the format \<_source dirname relative to build dir_\>:\<_target filepath_\> e.g. `../src/17.2/:www/docroot/js/chart-min.js`
            - Feature and firmware directories contain the directory structure for deployment on the target firmware.
- [minifier](https://github.com/seud0nym/tch-gui-unhide/tree/master/src/minifier)
    - A copy of [LuaSrcDiet](https://github.com/jirutka/luasrcdiet) that is temporarily deployed to the target device to minimise the Lua codebase when `tch-gui-unhide` is invoked with the `-my` option. Once the code has been minified, this code is deleted from the target device.
- [themes](https://github.com/seud0nym/tch-gui-unhide/tree/master/src/themes)
    - This directory contains the CSS files, images and icons that make up the themes. It also contains the Lua code that is included in the stock code to apply the themes.

## Building Releases

### `build`

The [build](https://github.com/seud0nym/tch-gui-unhide/blob/master/build/build) script, by default, will create all the installer scripts for the firmware directories found in the [build](https://github.com/seud0nym/tch-gui-unhide/tree/master/build) directory. You can modify this behaviour using the following options:

```
Usage: build/build [options] [target...]

Options:
 -p             Build a pre-release installer.
 -v nnnn.nn.nn  Build the installer with the specified version number.
                  If not specified, The version will be today's date.
 -D             Show the debugging messages as the build creates the installer.
                  Also runs commands such as `tar` with the verbose (`-v`) option.

target          One or more firmware versions for which the installer is to be built.
                  If not specified, an installer will be built for each directory 
                  in the build directory that starts with a number (e.g. 18.1.c)
```

The build process basically involves concatenating the individual script snippets into a single script, except for the `055-Additional` files. The contents of these files, plus the directives in the `.exclude` and `.include` files are resolved into a single tar file that is then base64 encoded and embedded in the installer script. When executed, the installer script unpacks the tar file into the firmware directories. The contents of the [themes](https://github.com/seud0nym/tch-gui-unhide/tree/master/src/themes) directory are handled in the same way for deployment on the target device.

The output of the [build](https://github.com/seud0nym/tch-gui-unhide/blob/master/build/build) script is to update the firmware specific `tch-gui-unhide-\<version\>` files in the root directory of the project.

### `build-release`

The [build-release](https://github.com/seud0nym/tch-gui-unhide/blob/master/build/build-release) script is used to create the tar release files for publishing on [Github](https://github.com/seud0nym/tch-gui-unhide/releases).

Before building a stable production release, the [VERSION.txt](https://github.com/seud0nym/tch-gui-unhide/blob/master/VERSION.txt) file must be updated with today's date, in the `YYYY.MM.DD` format. This file is checked by the individual installations to determine if an update is available. However, if you are building a pre-release, do *not* update [VERSION.txt](https://github.com/seud0nym/tch-gui-unhide/blob/master/VERSION.txt). Instead, add the `-p` parameter to build a pre-release.

```
Usage: build/build-release [options]

Options:
 -p             Build a pre-release.
```

The output of the [build-release](https://github.com/seud0nym/tch-gui-unhide/blob/master/build/build-release) script is firmware specific `\<version\>tar.gz` files in the root directory of the project.

## Testing

The only way to test changes is to deploy an installer to a Technicolor device and manually test your changes. To simplify the build/upload/install process, create the following script in the root directory of the project and call it `deploy` (this name is already ignored in [.gitignore](https://github.com/seud0nym/tch-gui-unhide/blob/master/.gitignore)):

```bash
#!/bin/bash

cd $(cd $(dirname $0); pwd)/build

unlock() { 
  if [ -n "$ip" ]; then
    echo
    echo -e "$ip \033[0;33mCtrl-C caught...performing clean up\033[0m"
    ssh root@$ip "lock -u /var/run/tch-gui-unhide.lck; rm -f /var/run/tch-gui-unhide.lck" 2> >(grep -v 'Warning: Permanently added')
  fi
}
trap "unlock" 2

IPS=""
PARAMS=""
BUILD_DATE=""
PRERELEASE=""
for p in $*; do
  if echo "$p" | grep -qE '^[[:digit:]]{4}\.[[:digit:]]{2}\.[[:digit:]]{2}$'; then
    BUILD_DATE="-v $p"
  elif echo "!$p" | grep -qE '^!-'; then
    [ "!$p" = "!-p" ] && PRERELEASE="-p" || PARAMS="$PARAMS $p"
  else
    IPS="$IPS $p"
  fi
done
if [ ! -z "$IPS" ]; then
  for ip in $IPS; do
    started=$(date +%r)
    fw=$(ssh root@$ip 'uci -q get version.@version[0].marketing_version' 2> >(grep -v 'Warning: Permanently added'))
    if echo "$fw" | grep -q "No route to host"; then
      echo $fw
      continue
    fi
    echo $ip Target firmware is $fw

    if [ ! -z "$fw" ]; then 
      if [ -n "$BUILD_DATE" -o ! -f ../tch-gui-unhide-$fw -o $(find ../build -newer ../tch-gui-unhide-$fw -type f ! -name deploy -a ! -name build-release -a ! -name README.md ! -name .source | wc -l) -gt 0 -o \( -z "$PRERELEASE" -a $(grep -c 'tch-gui-unhide Pre-Release [0-9.]* for Firmware' ../tch-gui-unhide-$fw) -ne 0 \) -o \( -n "$PRERELEASE" -a $(grep -c 'tch-gui-unhide Release [0-9.]* for Firmware' ../tch-gui-unhide-$fw) -ne 0 \) ]; then
        sh ./build $PRERELEASE $BUILD_DATE $fw
      else
        cd ../extras/src
        sh ./make
        cd - >/dev/null
      fi

      if [ ${ip#192.168} != $ip ]; then
        dir=$(ssh root@$ip 'for d in /mnt/usb/USB-* /root/mnt /root; do [ -d $d ] && echo $d; done | head -n 1' 2> >(grep -v 'Warning: Permanently added'))
      else
        dir=/root
      fi
      echo $ip Target directory is $dir

      echo $ip Syncing files...
      milliseconds=$(date +%s%3N)

      existing="$(ssh root@$ip "cd $dir;find . -maxdepth 1 -type f -size +0 -exec sha256sum {} \;" 2> >(grep -v 'Warning: Permanently added'))"
      changed=""

      files=$(ls -d ../tch-gui-unhide ../tch-gui-unhide-cards ../tch-gui-unhide-$fw ../utilities/* ../wifi-booster/* ../speedtest/* | grep -v -E 'restore-config|README|deprecated|after-reset|addTiming')
      [ $fw != 17.2 ] && files=$(echo $files | sed -e 's|../utilities/transformer-cli||')
      for f in $files; do
        if [ -f "$f" ]; then
          chmod +x "$f"
          [ "$(sha256sum "$f" | cut -d' ' -f1)" = "$(echo "$existing" | grep "\./$(basename "$f")$" | cut -d' ' -f1)" ] || changed="$changed $f"
        fi
      done

      files=$(ls -d ../extras/tch-gui-unhide-xtra* | grep -v 'extras/src')
      for f in $files; do
        chmod +x "$f"
        if ! grep -q "^$(basename "$f")$" ../extras/src/.extras; then
          if ! echo "$existing" | grep -q "\./$(basename "$f")$"; then
            continue
          fi
        fi
        [ "$(sha256sum "$f" | cut -d' ' -f1)" = "$(echo "$existing" | grep "\./$(basename "$f")$" | cut -d' ' -f1)" ] || changed="$changed $f"
      done
      
      [ -n "$changed" ] && scp -Cp $changed root@$ip:$dir 2> >(grep -v 'Warning: Permanently added')
      echo "$ip File sync took $(( $(date +%s%3N) - $milliseconds )) milliseconds"

      echo "$ip Running: tch-gui-unhide $PARAMS -y"
      ssh root@$ip "cd $dir;find . -maxdepth 1 -type f ! -name 'ipv*-DNS-Servers' ! -name authorized_keys -exec chmod +x {} \;;sh tch-gui-unhide $PARAMS -y" 2> >(grep -v 'Warning: Permanently added') | while read line; do echo "$ip $line"; done
    fi
    echo "$ip Started @ $started and Finished @ $(date +%r) (Directory=$dir)"
  done
fi
```

Execute this script with one or more IP addresses as parameters to deploy the latest code to a device or devices for testing.