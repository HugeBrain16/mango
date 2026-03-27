#include "color.h"
#include "string.h"

int color(const char *name) {
    if      (!strcmp(name, "white"))       return COLOR_WHITE;
    else if (!strcmp(name, "black"))       return COLOR_BLACK;
    else if (!strcmp(name, "green"))       return COLOR_GREEN;
    else if (!strcmp(name, "red"))         return COLOR_RED;
    else if (!strcmp(name, "blue"))        return COLOR_BLUE;
    else if (!strcmp(name, "yellow"))      return COLOR_YELLOW;
    else if (!strcmp(name, "pink"))        return COLOR_PINK;
    else if (!strcmp(name, "aqua"))        return COLOR_AQUA;
    else if (!strcmp(name, "orange"))      return COLOR_ORANGE;
    else if (!strcmp(name, "purple"))      return COLOR_PURPLE;
    else if (!strcmp(name, "darkgray"))    return COLOR_DARKGRAY;
    else if (!strcmp(name, "lightgray"))   return COLOR_LIGHTGRAY;
    else if (!strcmp(name, "transparent")) return COLOR_TRANSPARENT;
    return COLOR_INVALID;
}
