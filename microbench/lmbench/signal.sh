#!/bin/bash

source .env

# perform the benchmark
echo "Running lmbench lat_sig signal install..."
$lmbench_sig -P $PARALLEL -W $WARMUP -N $NTIMES install

echo "Running lmbench lat_sig signal catch (handler)..."
$lmbench_sig -P $PARALLEL -W $WARMUP -N $NTIMES catch
