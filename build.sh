#!/bin/bash

clang-format -i --style=Google main.cpp

mkdir -p build
cd build; 
cmake -DPICO_SDK_PATH=$(pwd)/../pico-sdk ..;
make;

mkdir -p ../_uf2/
cp radio_test.uf2 ../_uf2/