#! /bin/bash -e

source ../scripts/.env
ssh -p 10022 tdx@localhost "mkdir -p users"
scp -P 10022 ./* tdx@localhost:/home/tdx/users
