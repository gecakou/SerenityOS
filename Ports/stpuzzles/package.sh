#!/usr/bin/env -S bash ../.port_include.sh
port='stpuzzles'
version='c43a34fbfe430d235bafc379595761880a19ed9f'
files="https://git.tartarus.org/?p=simon/puzzles.git;a=snapshot;h=${version};sf=tgz puzzles-${version::7}.tar.gz 5e038b39d610d7ddf4bfa2f56bf626b79f9a43062f04357355d49f413b1a8853"
auth_type='sha256'
workdir="puzzles-${version::7}"
useconfigure='true'
configopts=(
    "-DCMAKE_TOOLCHAIN_FILE=${SERENITY_BUILD_DIR}/CMakeToolchain.txt"
    "-DCMAKE_CXX_FLAGS=-std=c++2a -O2"
)

configure() {
    run cmake "${configopts[@]}"
}

install() {
    run make install

    for puzzle in blackbox bridges cube dominosa fifteen filling flip flood galaxies guess inertia keen lightup loopy magnets map mines mosaic net netslide palisade pattern pearl pegs range rect samegame signpost singles sixteen slant solo tents towers tracks twiddle undead unequal unruly untangle; do
        install_launcher "${puzzle}" "Games/Puzzles" "/usr/local/bin/${puzzle}"
        install_icon "static-icons/${puzzle}.ico" "/usr/local/bin/${puzzle}"
    done
}
