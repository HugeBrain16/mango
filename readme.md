# Mango

![mango](https://i.imgur.com/zJ1AO2Y.png)

yummy kernel :3  

features:
- it boots
- keyboard input
- terminal (/w scrolling)
- basic commands
- memory file system (file editing is yet to be implemented)

commands:
- `scaleup` scale up font
- `scaledown` scale down font
- `clear` clear screen
- `echo ...` print text
- `fetch` print system info
- `list` list items in current folder
- `newfile` create a new file
- `copyfile` copy a file
- `movefile` move a file
- `delfile` delete a file
- `editfile` edit a file (todo)
- `newfolder` create a new folder
- `copyfolder` copy a folder
- `movefolder` move a folder
- `delfolder` delete a folder including the child items
- `goto` go into a folder
- `goup` go one step up from a folder
- `whereami` show current path
- `exit`

install i686-elf toolchain (Binutils, GCC, GDB) into "toolchain" folder
```sh
cd toolchain && source init.sh && cd ..
./build
./run
```
