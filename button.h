#ifndef BUTTON
#define BUTTON

#include <raylib.h>

typedef struct button
{
    char* label;
    int pressed;
    Rectangle rect;
} button;

button
b_init(char* label, int x, int y, int pressed);

void
b_draw(button* b);

// returns 1 if setting pressed to true
int
b_get_input(button* b);

#endif
