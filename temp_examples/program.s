format binary
use32
org 0xA0000000

db "MNGX"
dd start - 0xA0000000

start:
    mov eax, 1
    mov ebx, 2731
    mov ecx, msg
    mov edx, 14
    int 0x30
    mov eax, 0
    ret

msg db "hello world!", 0x0a, 0
