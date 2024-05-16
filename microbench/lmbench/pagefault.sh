#!/bin/bash

source .env

# perform the benchmark
echo "Running lmbench pagefault benchmark on a large file $LARGEFILE."
$lmbench_pagefault -P $PARALLEL -W $WARMUP -N $NTIMES $LARGEFILE
