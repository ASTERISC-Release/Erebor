#! /bin/bash -e

source ../scripts/.env
ssh -p 8000 $USERNAME@localhost "mkdir -p users"
scp -P 8000 ./* $USERNAME@localhost:/home/$USERNAME/users