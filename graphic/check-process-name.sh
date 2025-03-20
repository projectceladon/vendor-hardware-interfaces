#!/bin/bash
IP=10.239.115.11:5555
touch before
touch after
adb connect $IP
adb root
adb connect $IP
adb shell lsof /dev/dri/renderD128 > before
touch diff_process
while true; do
    adb shell lsof /dev/dri/renderD128 > after
    diff before after > diff_process
    awk '{print $2}' diff_process | sort -u
    rm diff_process
    touch diff_process
    cp after before
    rm after
    touch after

    sleep 2
done
