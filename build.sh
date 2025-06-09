#!/bin/bash

clang-format -i --style=Google main.cpp

cd build; 
cmake -DPICO_SDK_PATH=$(pwd)/../pico-sdk ..;
make;

cp radio_test.uf2 /mnt/c/Users/tyler/Documents/Coconut/_uf2/