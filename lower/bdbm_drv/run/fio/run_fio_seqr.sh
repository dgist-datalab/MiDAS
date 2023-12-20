#!/bin/bash

sudo fio --randrepeat=1 \
	--ioengine=libaio \
	--name=fio\
	--filename=/media/blueDBM/fio \
	--bs=4k \
	--iodepth=128 \
	--size=10000M \
	--readwrite=read \
	--rwmixwrite=0 \
	--rwmixread=100 \
	--overwrite=1 \
	--numjobs=8 \
	--direct=0 \
	--buffer=1
