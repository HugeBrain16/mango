#!/usr/bin/env bash

if [[ $MANGO_TOOLCHAIN -ne 1 ]]; then
    echo "Not using toolchain, exiting..."
    exit 1
fi

if [[ $# -eq 0 ]]; then
    echo "Usage: $0 <sources>"
    exit 1
fi

CC=""
if command -v ccache >/dev/null 2>&1; then
    CC="ccache "
fi
CC="$CC i686-elf-gcc"

CFLAGS="$CFLAGS -DMANGO_COMPAT -std=gnu99 -ffreestanding -O3 -g -Wall -Wextra
    -fno-align-functions -ffunction-sections -fdata-sections"
DIR="$(mktemp -t mangoprog.XXXX -d)"

compile() {
    echo "Compiling $1..."
    out=$(echo "$1" | sed 's|^src/||; s|\.c$|.o|')
    mkdir -p $(dirname "$DIR/$out")
    $CC -c $1 -Idev -o $DIR/$out $CFLAGS
    OBJECTS="$OBJECTS $DIR/$out"
}

assemble() {
    echo "Compiling $1..."
    out=$(echo "$1" | sed 's|^src/||; s|\.s$|.o|')
    mkdir -p $(dirname "$DIR/$out")
    i686-elf-as $1 -o $DIR/$out
    OBJECTS="$OBJECTS $DIR/$out"
}

for asm in $(find "$@" -type f -name "*.s"); do
    assemble "$asm"
done

for kernel_src in $(find "$@" -type f -name "*.c"); do
    compile "$kernel_src"
done

echo "Objects ($OBJECTS )"
echo "Linking program..."

OUT="$(basename "${1%.*}")"
if [[ -v PROGRAM ]]; then
    OUT="$PROGRAM"
fi

$CC -T dev/linker.ld -o $DIR/$OUT.bin -nostdlib $CFLAGS -Wl,--gc-sections -Wl,--no-warn-rwx-segments $OBJECTS

if [[ $? -ne 0 ]]; then
    exit 1
fi

i686-elf-objcopy -O binary $DIR/$OUT.bin $OUT.mx

echo "Done! cleanup..."
rm -r $DIR

