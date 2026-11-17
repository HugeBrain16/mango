#include "syscall.h"
#include "string.h"
#include "serial.h"
#include "file.h"
#include "screen.h"
#include "pit.h"
#include "exec.h"

void syscall_handle(int_frame_t *frame) {
    switch (frame->eax) {
        case CALL_SERIAL_WRITE:
            serial_write((const char*)frame->ebx);
            break;
        case CALL_FILE_WRITE:
        {
            int r = file_write((uint32_t)frame->ebx, (const char*)frame->ecx, (size_t)frame->edx);
            frame->eax = (uint32_t)r;
            break;
        }
        case CALL_FILE_READ:
        {
            int r = file_read((uint32_t)frame->ebx, (char*)frame->ecx, (size_t)frame->edx);
            frame->eax = (uint32_t)r;
            break;
        }
        case CALL_SYS_PERF:
        {
            double perf = (double)pit_ticks / (double)pit_hz;
            asm volatile("fldl %0" :: "m"(perf));
            break;
        }
        case CALL_KEY_DOWN:
        {
            frame->eax = (uint32_t)exec_key;
            break;
        }
        case CALL_SCREEN_FLUSH:
            screen_flush();
            break;
        case CALL_SCREEN_CLEAR:
            screen_clear((uint32_t)frame->ebx);
            break;
        case CALL_SCREEN_DRAW:
            screen_draw_pixel((int)frame->ebx, (int)frame->ecx, (uint32_t)frame->edx, (int)frame->esi);
            break;
        case CALL_SCREEN_DRAW_CHAR:
        {
            float scale;
            asm volatile("" : "=t"(scale) :: "memory");
            screen_draw_char((int)frame->ebx, (int)frame->ecx, (char)frame->edx, (uint32_t)frame->esi, (uint32_t)frame->edi, scale);
            break;
        }
        case CALL_SCREEN_INFO:
            *(uint32_t*)frame->ebx = screen_width;
            *(uint32_t*)frame->ecx = screen_height;
            *(uint32_t*)frame->edx = screen_pitch;
            asm volatile("flds %0" :: "m"(screen_scale));
            break;
    }
}
