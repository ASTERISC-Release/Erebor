#!/bin/bash

source .env

# perform the benchmark
echo "Running lmbench syscal benchmark for open+close on a largefile $LARGEFILE."
$lmbench_syscall -P $PARALLEL -W $WARMUP -N $NTIMES open $LARGEFILE

echo "Running lmbench syscal benchmark for null (getpid)."
$lmbench_syscall -P $PARALLEL -W $WARMUP -N $NTIMES null
