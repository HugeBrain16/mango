#include "syscall.h"
#include "string.h"
#include "serial.h"
#include "file.h"

enum Call {
    CALL_SERIAL_WRITE,
    CALL_FILE_WRITE,
    CALL_FILE_READ,
};

void syscall_handle(int_frame_t *frame) {
    switch (frame->eax) {
        case CALL_SERIAL_WRITE:
            serial_write((const char*)frame->ebx);
            break;
        case CALL_FILE_WRITE:
            file_write((uint32_t)frame->ebx, (const char*)frame->ecx, (size_t)frame->edx);
            break;
        case CALL_FILE_READ:
            file_read((uint32_t)frame->ebx, (char*)frame->ecx, (size_t)frame->edx);
            break;
    }
}
