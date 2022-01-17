#!/bin/sh

log I "Restoring passwords and SSH keys..."
restore_file /etc/group /etc/passwd /etc/shadow /etc/config/dropbear /etc/dropbear/* /etc/rc.d/*dropbear
