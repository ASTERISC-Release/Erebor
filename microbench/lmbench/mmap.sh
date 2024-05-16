#!/bin/bash

source .env

FSIZE="4m"

# perform the benchmark
echo "Running lmbench mmap benchmark..."
echo "$lmbench_mmap -P $PARALLEL -W $WARMUP -N $NTIMES $FSIZE $LARGEFILE"
echo "Output format is CB"%0.2f %d\n", megabytes, usecs."
$lmbench_mmap -P $PARALLEL -W $WARMUP -N $NTIMES $FSIZE $LARGEFILE
