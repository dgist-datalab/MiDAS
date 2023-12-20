#!/bin/bash

sudo mkdir -p /usr/share/bdbm_drv
sudo touch /usr/share/bdbm_drv/ftl.dat
sudo touch /usr/share/bdbm_drv/dm.dat

sudo insmod bdbm_drv.ko
sleep 1
sudo ./libftl &
sleep 4
sudo mkfs -t ext4 -b 4096 /dev/blueDBM
sudo mount \-t ext4 \-o discard /dev/blueDBM /media/blueDBM
