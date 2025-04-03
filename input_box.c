#include "input_box.h"
#include "raylib.h"
#include <ctype.h>
#include <limits.h>
#include <raylib.h>
#include <stdlib.h>
#include <string.h>

int
ib_get_text_as_integer(input_box* ib)
{
    if (ib->text[0] == '\0')
        return 0;

    char* endPtr;
    long result = strtol(ib->text, &endPtr, 10);

    if (*endPtr != '\0' && !isspace((unsigned char)*endPtr))
        return 0;

    if (result > INT_MAX || result < INT_MIN)
        return 0;

    return (int)result;
}

float
ib_get_text_as_float(input_box* ib)
{
    if (ib->text[0] == '\0')
        return 0;

    char* endPtr;
    float result = strtof(ib->text, &endPtr);

    if (*endPtr != '\0' && !isspace((unsigned char)*endPtr))
        return 0;

    return result;
}

char*
ib_get_text_as_string(input_box* ib)
{
    return ib->text;
}

int
ib_get_input(input_box* ib)
{
    if (!ib->focused)
        return 0;

    int modified_text = 0;

    int ch = GetCharPressed();
    while (ch > 0) {
        if ((ch >= 32) && (ch <= 125) && (ib->text_size < MAX_TEXT_SIZE - 1)) {
            ib->text[ib->text_size] = (char)ch;
            ib->text[ib->text_size + 1] = '\0';
            ib->text_size++;

            modified_text = 1;
        }

        // get next in queue
        ch = GetCharPressed();
    }

    if ((IsKeyPressed(KEY_BACKSPACE)) && ib->text_size > 0) {
        ib->text_size--;
        ib->text[ib->text_size] = '\0';

        return 1;
    }

    return modified_text;
}

void
ib_check_focus(input_box* ib)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return;

    if (CheckCollisionPointRec(GetMousePosition(), ib->rect))
        ib->focused = true;
    else
        ib->focused = false;
}

void
ib_draw(input_box* ib)
{
    DrawRectangleRec(ib->rect, ib->focused ? LIGHTGRAY : GRAY);

    DrawText(ib->label, ib->rect.x, ib->rect.y - 20, 20, GRAY);
    DrawText(ib->text, ib->rect.x + 5, ib->rect.y + 2, 20, BLACK);
}

input_box
ib_init(char* label, int x, int y, char* init_text)
{
    input_box ib;

    ib.focused = false;

    int end = 0;
    for (int i = 0; i < MAX_TEXT_SIZE - 1; i++) {
        if (init_text[i] == '\0') {
            end = 1;
        }

        if (!end) {
            ib.text[i] = init_text[i];
            continue;
        }

        ib.text[i] = ' ';
    }
    ib.text[MAX_TEXT_SIZE - 1] = '\0';
    ib.text_size = strlen(init_text);
    ib.rect = (Rectangle){ x, y, 100, 20 };
    ib.label = label;

    return ib;
}
