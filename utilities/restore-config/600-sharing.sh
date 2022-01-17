#!/bin/sh

log I "Restoring file sharing configuration..."
restore_file /etc/config/samba
if [ "$BACKUP_VERSION" != "17.2" -a "$BACKUP_VERSION" != "18.1.c" ]; then
  restore_directory /etc/nqe
else
  restore_file /etc/samba/smbpasswd
fi
log I "Restoring content sharing configuration..."
restore_file /etc/config/dlnad
log I "Restoring printer sharing configuration..."
restore_file /etc/config/printersharing
