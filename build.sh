#!/bin/bash

cd build; 
cmake ..;
make;
cp radio_test.uf2 ../_uf2/