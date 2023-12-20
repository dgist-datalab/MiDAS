#! /bin/bash
sudo ifconfig enp1s0 down
sudo ifconfig enp1s0 mtu $1
sudo ifconfig enp1s0 up
