#!/usr/bin/env bash

export MANGO_TOOLCHAIN=1
export PREFIX="$(pwd)/toolchain"
export PATH="$PREFIX/bin:$PATH"
export TARGET=i686-elf
export SYSROOT="$PREFIX/$TARGET"
