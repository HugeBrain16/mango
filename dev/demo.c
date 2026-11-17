#include <mango.h>
#include <color.h>

int width;
int height;

int start() {
    screen_info(&width, &height, NULL, NULL);
    screen_clear(COLOR_BLUE);
    screen_flush();
    return 0;
}
