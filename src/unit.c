#include "string.h"

void unit_get_size(uint32_t size, char *buffer) {
    if ((size >> 30) > 0)
        return strfmt(buffer, "%d GB", size >> 30);
    else if ((size >> 20) > 0)
        return strfmt(buffer, "%d MB", size >> 20);
    else if ((size >> 10) > 0)
        return strfmt(buffer, "%d KB", size >> 10);

    return strfmt(buffer, "%d B", size);
}
