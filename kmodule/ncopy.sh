#! /bin/bash -e

source ../scripts/.env
ssh -p 8000 pks@localhost "mkdir -p kmod"
scp -P 8000 ./* $USERNAME@localhost:/home/$USERNAME/kmod