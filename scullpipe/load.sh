#!/bin/bash
module="scullpipe"
device="scullpipe"
mode="666"

/sbin/insmod ./${module}.ko $* || exit 1

rm -rf /dev/${device}

major=$(awk -v device="$device" '$2 == device {print $1}' /proc/devices | awk 'NR == 1')

mknod /dev/${device} c $major 0

group="root"
grep -q "root" /etc/group || group="wheel"

chgrp $group /dev/${device}
chmod $mode /dev/${device}

