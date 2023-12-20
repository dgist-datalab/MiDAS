#!/bin/bash

sudo fio --randrepeat=1 \
	--name=libaio\
	--filename=/media/blueDBM/fio \
	--bs=4k \
	--iodepth=128 \
	--size=10000M \
	--readwrite=randread \
	--rwmixwrite=0 \
	--rwmixread=100 \
	--overwrite=1 \
	--numjobs=8 \
	--direct=0 \
	--buffer=1
