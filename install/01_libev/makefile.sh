#!/usr/bin/env bash

filepath=$(cd "$(dirname "$0")"; pwd)

cd ${filepath}/4.03/
./autogen.sh
./configure
make
sudo make install

#����ϵͳ��
sudo ldconfig

