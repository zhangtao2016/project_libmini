#!/usr/bin/env bash

filepath=$(cd "$(dirname "$0")"; pwd)

cd ${filepath}/master/
./autogen.sh
./configure
make
sudo make install

#更新系统库
sudo ldconfig

