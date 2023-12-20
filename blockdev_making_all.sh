#!/bin/bash
make TARGET_ALGO=DFTL cheeze_block_driver -j
mv cheeze_block_driver ./result/cheeze_dftl_driver
make clean

make TARGET_ALGO=lsmftl cheeze_block_driver -j
mv cheeze_block_driver ./result/cheeze_lsmftl_driver
make clean

make TARGET_ALGO=Page_ftl cheeze_block_driver -j
mv cheeze_block_driver ./result/cheeze_page_driver
make clean
