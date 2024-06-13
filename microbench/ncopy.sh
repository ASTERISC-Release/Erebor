#! /bin/bash -e

PORT="$1"

if [[ -z $PORT ]]; then
	PORT=8000
fi

source ../scripts/.env
ssh -p $PORT $USERNAME@localhost "mkdir -p microbench/"
scp -r -P $PORT ./* $USERNAME@localhost:/home/$USERNAME/microbench
