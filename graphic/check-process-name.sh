#!/bin/bash

trap 'cleanup_and_exit' SIGINT

cleanup_and_exit() {
    echo -e "\nDetect Ctrl+C，clean up the temporary files..."
    rm -f diff_process process_name after before
    echo "Add the following process names to cfg file, excluding the system related process, like:
    system_server, ndroid.systemui, hardware.intel. etc."
    cat add_process
    rm add_process
    exit 0
}

# Check if a parameter is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <Device IP:PORT or Serial Number>"
    exit 1
fi

device=$1

if [ -f "$before" ]; then
  rm -f "$before"
fi

if [ -f "$after" ]; then
  rm -f "$after"
fi

if [ -f "$diff_process" ]; then
  rm -f "$diff_process"
fi

if [ -f "$process_name" ]; then
  rm -f "$process_name"
fi

if [ -f "$add_process" ]; then
  rm -f "$add_process"
fi

# Determine if the input is an IP address or a device serial number
if [[ $device =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+(:[0-9]+)?$ ]]; then
    echo "Attempting to connect to network device: $device"
    adb connect $device
    adb -s $device root
    adb connect $device
else
    echo "Attempting to connect to USB device: $device"
    adb -s $device root
fi

if [ $? -ne 0 ]; then
    echo "device connect fail, exit!"
    exit 1
fi

touch before
touch after
adb -s $device shell lsof /dev/dri/renderD128 > before
touch diff_process
touch process_name
touch add_process
while true; do
    adb -s $device shell lsof /dev/dri/renderD128 > after
    diff before after > diff_process
    awk '{print $2}' diff_process | sort -u >process_name
    while read -r line; do
        if ! grep -Fxq "$line" add_process; then
            echo "$line" >> add_process
        fi
    done < process_name

    rm diff_process
    touch diff_process
    rm process_name
    touch process_name
    cp after before
    rm after
    touch after

    sleep 2
done
