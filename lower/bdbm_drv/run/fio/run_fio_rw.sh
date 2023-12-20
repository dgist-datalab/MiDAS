#!/bin/bash

#sudo fio --randrepeat=1 \
	#--ioengine=libaio \
	#--buffered=1 \
	#--name=fio\
	#--filename=/media/blueDBM/fio \
	#--bs=4k \
	#--iodepth=128 \
	#--size=1000M \
	#--readwrite=randrw \
	#--rwmixread=0 \
	#--overwrite=1 \
	#--numjobs=8 \
	#--fsync=1000

sudo rm /media/blueDBM/fio

sudo fio --randrepeat=1 \
	--ioengine=libaio \
	--name=fio\
	--filename=/media/blueDBM/fio \
	--bs=4k \
	--iodepth=128 \
	--size=1000M \
	--readwrite=randrw \
	--rwmixread=0 \
	--overwrite=0 \
	--numjobs=8 \
	--direct=0 \
	--buffered=0

