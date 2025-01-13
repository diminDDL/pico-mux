#!/bin/bash
found_device=false
for device in /dev/ttyACM*
do
    if [ -e "$device" ]
    then
        stty -F $device 1200
        found_device=true
    else
        echo "No device matching $device found."
    fi
done

if [ "$found_device" = false ] ; then
    echo "No devices matching /dev/ttyACM* found."
fi