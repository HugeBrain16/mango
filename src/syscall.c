#include "syscall.h"
#include "string.h"
#include "serial.h"
#include "file.h"
#include "screen.h"

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
        case CALL_SCREEN_FLUSH:
            screen_flush();
            break;
        case CALL_SCREEN_DRAW:
            screen_draw_pixel((int)frame->ebx, (int)frame->ecx, (uint32_t)frame->edx, (int)frame->esi);
            break;
    }
}
