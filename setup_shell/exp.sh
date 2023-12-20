#!/bin/bash
mkdir -f ../result
for entry in $1/*
do 
	name=$(basename $entry)
	./$entry > ../result/$name.out
done
