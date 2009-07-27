#!/bin/bash
# Synchronises a directory off the logic analyser onto this machine
# Handy when you're analysing stuff 'live' 
# Uses rsync, so you can just keep sync'ing the same directory, and it
# will only copy what has changed.
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
