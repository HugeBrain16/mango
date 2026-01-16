# Mango OS

![mango](https://i.imgur.com/5RssEB5.png)

features:
- it boots
- keyboard input
- terminal (/w scrolling)
- basic commands
- custom file system
- basic text editor

<details>
<summary>commands</summary>

- `scaleup` scale up font
- `scaledown` scale down font
- `clear` clear screen
- `echo ...` print text
- `fetch` print system info
- `formatdisk` format primary disk drive
- `list` list items in current folder
- `newfile` create a new file
- `copyfile` copy a file
- `movefile` move a file
- `delfile` delete a file
- `edit` edit a file
- `newfolder` create a new folder
- `copyfolder` copy a folder
- `movefolder` move a folder
- `delfolder` delete a folder including the child items
- `goto` go into a folder
- `goup` go one step up from a folder
- `whereami` show current path
- `exit`
</details>

install i686-elf toolchain (Binutils, GCC, GDB) into "toolchain" folder
```sh
source toolchain/init.sh
./build
./run
```
