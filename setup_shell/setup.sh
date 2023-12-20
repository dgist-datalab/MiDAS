#!/bin/bash
if [ "$1" = "c" ]; then
	make -j4
elif [ "$1" = "s" ]; then
	make -f Net.Makefile -j4
elif [ "$1" = "A" ]; then
	make -j4
	make -f Net.Makefile -j4
	./mtu_up.sh
elif [ "$1" = "r" ]; then
	make clean
	make -f Net.Makefile clean
fi
