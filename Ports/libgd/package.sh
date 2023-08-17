#!/usr/bin/env -S bash ../.port_include.sh
port=libgd
version=2.3.3
useconfigure=true
use_fresh_config_sub=true
configopts=(
    "--with-sysroot=${SERENITY_INSTALL_ROOT}"
    '--without-avif'
    '--without-heif'
    '--without-liq'
    '--without-raqm'
    '--without-webp'
    '--without-x'
    '--without-xpm'
)
config_sub_paths=("config/config.sub")
files=(
    "https://github.com/libgd/libgd/releases/download/gd-${version}/libgd-${version}.tar.gz dd3f1f0bb016edcc0b2d082e8229c822ad1d02223511997c80461481759b1ed2"
)
depends=(
    'fontconfig'
    'freetype'
    'libjpeg'
    'libpng'
    'libtiff'
    'zlib'
)
