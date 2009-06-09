#!/bin/bash
IP=192.168.2.90
SYNC=$1
if [ -z "$1" ] ; then
    echo "Enter directory name on TLA5204 to sync (relative to C:/My Documents"
    exit 1
fi
BASENAME=`basename "$SYNC"`
sudo mount -t cifs //$IP/My\ Documents /mnt -n -o user=guest,sec=none,ro
rsync -avq "/mnt/$SYNC/" "$BASENAME"
sudo umount /mnt
