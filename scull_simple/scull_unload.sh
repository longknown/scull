#!/bin/sh

module="scull"
device="scull"

# rm module
rmmod $module

# remove device files
rm -rf /dev/${device}[0-3]
