#!/bin/bash

source .env

file_sizes=("1M")
file_prefix="testfile_"

# create file
for size in "${file_sizes[@]}"; do
        file_name="${file_prefix}${size}.tmp"
        if [ ! -f $file_name ]; then
                dd if=/dev/urandom of=$file_name bs=$size count=1
        fi

        # benchmark testing
        echo "Running lmbench pagefault benchmark on a file $file_name."
        $lmbench_pagefault -P 4 -W $WARMUP -N $NTIMES $file_name
done
