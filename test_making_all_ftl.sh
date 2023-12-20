#!/bin/bash
make TARGET_ALGO=DFTL driver -j
mv driver ./result/dftl_driver
make clean

make TARGET_ALGO=lsmftl driver -j
mv driver ./result/lsmftl_driver
make clean

make TARGET_ALGO=Page_ftl driver -j
mv driver ./result/page_driver
make clean
