#include "button.h"
#include <raylib.h>

void
b_draw(button* b)
{
    DrawRectangle(b->rect.x, b->rect.y, 20, 20, b->pressed ? LIGHTGRAY : GRAY);
    DrawText(b->label, b->rect.x + 25, b->rect.y, 20, GRAY);
}

int
b_get_input(button* b)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return 0;

    if (CheckCollisionPointRec(GetMousePosition(), b->rect)) {
        b->pressed = true;
        return 1;
    }

    return 0;
}

button
b_init(char* label, int x, int y, int pressed)
{
    button b;
    b.pressed = pressed;
    b.label = label;
    b.rect = (Rectangle){ x, y, 40, 20 };
    return b;
}
