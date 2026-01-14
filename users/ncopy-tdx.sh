#! /bin/bash -e

source ../scripts/.env
ssh -p 8000 tdx@localhost "mkdir -p users"
scp -P 8000 ./* tdx@localhost:/home/tdx/users
