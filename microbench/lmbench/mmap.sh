#!/bin/bash

source .env

FSIZE="4m"


source .env
file_sizes=("1M")
file_prefix="testfile_"

echo "Output format is CB"%0.2f %d\n", megabytes, usecs."

# create file
for size in "${file_sizes[@]}"; do
    # benchmark testing
        echo "Running lmbench mmap benchmark... stride=16K, fsize=$size."
        $lmbench_mmap -P $PARALLEL -W $WARMUP -N $NTIMES $size $LARGEFILE
done


