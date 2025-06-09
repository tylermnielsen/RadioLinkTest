# RadioLinkTest
Script for radio link testing for the Coconut CubeSat

## Setup
Run [`setup.sh`](/setup.sh) to set up submodules (RadioLib and my fork of pico-sdk with changed settings)

## Build 
Run [`build.sh`](/build.sh) to build the project. This will create all of the build folder and copy the resulting .uf2 file in a [_uf2](/_uf2) folder which can then be uploaded to the RP2040 in bootsel mode. 

