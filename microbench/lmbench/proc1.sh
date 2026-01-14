#!/bin/bash

source .env

# perform the benchmark
echo "Running lmbench process benchmark for fork+exit."
$lmbench_proc -P 4 -W 1 -N $NTIMES fork

#echo "Running lmbench process benchmark for fork+execve."
#$lmbench_proc -P 4 -W 1 -N $NTIMES exec
