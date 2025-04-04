#ifndef SLIDE_BAR
#define SLIDE_BAR

#include <raylib.h>

typedef struct slide_bar
{
    char* label;
    int start_x;
    int end_x;
    int y;
    int nob_x;
} slide_bar;

// start has to be smaller than end, and nob has to be in between
slide_bar
sb_init(char* label, int start_x, int end_x, int y, int nob_x);

void
sb_draw(slide_bar* sb);

float
sb_get_ratio(slide_bar* sb);

// return 1 if nob_x has been updated
int
sb_get_input(slide_bar* sb);

#endif
