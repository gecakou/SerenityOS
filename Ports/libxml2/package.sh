#!/usr/bin/env -S bash ../.port_include.sh
port='libxml2'
version='2.11.5'
files=(
    "https://download.gnome.org/sources/libxml2/2.11/libxml2-${version}.tar.xz#3727b078c360ec69fa869de14bd6f75d7ee8d36987b071e6928d4720a28df3a6"
)
useconfigure='true'
use_fresh_config_sub='true'
configopts=(
    "--with-sysroot=${SERENITY_INSTALL_ROOT}"
    '--prefix=/usr/local'
    '--without-python'
    '--disable-static'
    '--enable-shared'
)
depends=(
    'libiconv'
    'xz'
)
