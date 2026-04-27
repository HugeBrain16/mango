#include <math.h>

float powf(float x, int y) {
    float r = 1.0f;
    int n = (y < 0) ? -y : y;

    while (n--)
        r *= x;

    return (y < 0) ? 1.0f / r : r;
}