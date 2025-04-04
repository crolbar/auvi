#include "util.h"

int
min(int x, int y)
{
    if (x > y) {
        return y;
    }
    return x;
}

int
max(int x, int y)
{
    if (x < y) {
        return y;
    }
    return x;
}

int
clamp(int v, int low, int hight)
{
    return min(hight, max(low, v));
}

float
minf(float x, float y)
{
    if (x > y) {
        return y;
    }
    return x;
}

float
maxf(float x, float y)
{
    if (x < y) {
        return y;
    }
    return x;
}

float
clampf(float v, float low, float hight)
{
    return minf(hight, maxf(low, v));
}
