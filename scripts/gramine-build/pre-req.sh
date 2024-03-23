#!/bin/bash

sudo apt-get install -y build-essential \
    autoconf bison gawk nasm ninja-build pkg-config python3 python3-click \
    python3-jinja2 python3-pip python3-pyelftools wget
sudo python3 -m pip install 'meson>=0.56' 'tomli>=1.1.0' 'tomli-w>=0.4.0'

sudo apt-get install -y libunwind8 musl-tools python3-pytest

sudo apt-get install -y libgmp-dev libmpfr-dev libmpc-dev libisl-dev
