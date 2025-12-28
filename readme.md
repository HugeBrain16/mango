# Mango

![mango](https://i.imgur.com/zJ1AO2Y.png)

yummy kernel :3  

features:
- it boots
- keyboard input
- terminal (/w scrolling)
- basic commands

commands:
- `scaleup` scale up font
- `scaledown` scale down font
- `clear` clear screen
- `echo ...` print text
- `fetch` print system info
- `exit`

install i686-elf toolchain (Binutils, GCC, GDB) into "toolchain" folder
```sh
cd toolchain && source init.sh && cd ..
./build
./run
```
