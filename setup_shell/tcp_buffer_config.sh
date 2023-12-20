#! /bin/bash

sysctl -w net.core.rmem_default="253952"
sysctl -w net.core.wmem_default="253952"
sysctl -w net.core.rmem_max="16777216"
sysctl -w net.core.wmem_max="16777216"
sysctl -w net.ipv4.tcp_rmem="253952 253952 16777216"
sysctl -w net.ipv4.tcp_wmem="253952 253952 16777216"
sysctl -w net.ipv4.tcp_rfc1337=1
