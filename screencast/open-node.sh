#!/bin/bash

value=$(getprop | grep 'ro.boot.vm' | awk -F'[][]' '{print $4}')

if [ "$value" == "1" ]; then
    echo "VM1"
    echo "1af4 1110 8086 202" > /sys/bus/pci/drivers/ivshm_ivshmem/new_id
    chmod 666 /dev/ivshm0*
elif [ "$value" == "2" ]; then
    echo "VM2"
    echo "1af4 1110 8086 200" > /sys/bus/pci/drivers/ivshm_ivshmem/new_id
    chmod 666 /dev/ivshm0*
    echo "1af4 1110 8086 201" > /sys/bus/pci/drivers/ivshm_ivshmem/new_id
    chmod 666 /dev/ivshm1*
else
    echo "no ivshm config"
fi

# Set udmabuf list limit and change permissions
echo 8192 > /sys/module/udmabuf/parameters/list_limit
cat /sys/module/udmabuf/parameters/list_limit
chmod 666 /dev/udmabuf

# Create IPC directory and change permissions

SOCKET_DIR="/dev/socket"
SOCKET_SERVER="$SOCKET_DIR/virt_disp_server"
SOCKET_CLIENT="$SOCKET_DIR/virt_disp_client"

if [ -f "$SOCKET_SERVER" ]; then
  rm -f "$SOCKET_SERVER"
fi
touch "$SOCKET_SERVER"
chmod 777 "$SOCKET_SERVER"

if [ -f "$SOCKET_CLIENT" ]; then
  rm -f "$SOCKET_CLIENT"
fi
touch "$SOCKET_CLIENT"
chmod 777 "$SOCKET_CLIENT"

restorecon "$SOCKET_SERVER"
restorecon "$SOCKET_CLIENT"

chown root /system/bin/probe-node
chmod u+s /system/bin/probe-node
