#!/bin/bash

pushd ../
    source .env
popd

if [ ! -d $GRAMINEDIR ]; then
    cd $GRAMINEDIR/../
    git@github.com:Icegrave0391/gramine.git
fi