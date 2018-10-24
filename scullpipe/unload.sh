#!/bin/bash
module="scullpipe"
device="scullpipe"

rmmod $module
rm -rf /dev/$device
