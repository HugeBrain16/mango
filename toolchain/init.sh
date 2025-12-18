#!/usr/bin/env bash

export PREFIX="$(pwd)/toolchain"
export PATH="$PREFIX/bin:$PATH"
export TARGET=i686-elf
export SYSROOT="$PREFIX/$TARGET"
