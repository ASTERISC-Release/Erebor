#! /bin/bash -e

source ../scripts/.env
ssh -p 8000 $USERNAME@localhost "mkdir -p microbench/"
scp -r -P 8000 ./* $USERNAME@localhost:/home/$USERNAME/microbench
