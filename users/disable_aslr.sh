#! /bin/bash -e

source ../scripts/.env
ssh -p 8000 $USERNAME@localhost "sudo sysctl kernel.randomize_va_space=0"