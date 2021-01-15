#!/bin/bash ../.port_include.sh
port=milkytracker
version=1.03.00
workdir=MilkyTracker-$version
useconfigure=true
files="https://github.com/milkytracker/MilkyTracker/archive/v$version.tar.gz MilkyTracker-$version.tar.gz"
configopts="-DCMAKE_TOOLCHAIN_FILE=$SERENITY_ROOT/Toolchain/CMakeToolchain.txt"
makeopts="-I../../SDL2/SDL-master-serenity/include/ -lSDL2 -j$(nproc)"
installopts=""
depends="SDL2"

configure() {
    run cmake $configopts
}
