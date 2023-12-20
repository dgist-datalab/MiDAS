#!/bin/bash

sysctl -w net.ipv4.tcp_low_latency=1
sysctl -w net.ipv4.tcp_sack=0
sysctl -w net.ipv4.tcp_timestamps=0
