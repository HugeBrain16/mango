#include "string.h"

void unit_get_size(uint32_t size, char *buffer) {
    if ((size >> 30) > 0) {
        uint32_t whole = size >> 30;
        uint32_t frac  = ((size - (whole << 30)) * 100) >> 30;
        return strfmt(buffer, "%d.%d GB", whole, frac);
    } else if ((size >> 20) > 0) {
        uint32_t whole = size >> 20;
        uint32_t frac  = ((size - (whole << 20)) * 100) >> 20;
        return strfmt(buffer, "%d.%d MB", whole, frac);
    } else if ((size >> 10) > 0) {
        uint32_t whole = size >> 10;
        uint32_t frac  = ((size - (whole << 10)) * 100) >> 10;
        return strfmt(buffer, "%d.%d KB", whole, frac);
    }
    return strfmt(buffer, "%d B", size);
}
