#include "slide_bar.h"
#include "raylib.h"
#include "util.h"
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>

slide_bar
sb_init(char* label, int start_x, int end_x, int y, int nob_x)
{
    slide_bar sb;
    sb.label = label;
    sb.start_x = start_x;
    sb.end_x = end_x;
    sb.nob_x = nob_x;
    sb.y = y;
    return sb;
}

Rectangle
get_nob_rect(int nob_x, int y)
{
    return (Rectangle){ nob_x - 5, y - 15, 10, 30 };
}

float
sb_get_ratio(slide_bar* sb)
{
    return (float)(sb->nob_x - sb->start_x) / (sb->end_x - sb->start_x);
}

void
sb_draw(slide_bar* sb)
{
    Rectangle barRect1 =
      (Rectangle){ sb->start_x - 5, sb->y - 10, sb->nob_x - sb->start_x, 20 };
    Rectangle barRect2 = (Rectangle){
        sb->nob_x, sb->y - 10, sb->end_x - sb->nob_x - 5 + 5 + 5, 20
    };
    Rectangle nobRect = get_nob_rect(sb->nob_x, sb->y);

    DrawRectangleRec(barRect1, RED);
    DrawRectangleRec(barRect2, DARKGRAY);
    DrawRectangleRec(nobRect, GRAY);

    int text_len = MeasureText(sb->label, 20);
    DrawText(sb->label,
             sb->start_x + ((sb->end_x - sb->start_x) / 2) - text_len / 2,
             sb->y - 20 - 10,
             20,
             GRAY);

    char s[5];
    sprintf(s, "%.02f", (float)(sb->nob_x - sb->start_x) / (sb->end_x - sb->start_x));
    text_len = MeasureText(s, 20);
    DrawText(s,
             sb->start_x + ((sb->end_x - sb->start_x) / 2) - text_len / 2,
             sb->y - 10,
             20,
             LIGHTGRAY);
}

int
sb_get_input(slide_bar* sb)
{
    Rectangle nobRect = get_nob_rect(sb->nob_x, sb->y);
    Rectangle barRect =
      (Rectangle){ sb->start_x, sb->y - 10, sb->end_x - sb->start_x, 20 };

    Vector2 mp = GetMousePosition();
    if (!CheckCollisionPointRec(mp, nobRect) &&
        !CheckCollisionPointRec(mp, barRect))
        return 0;

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        !IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return 0;

    sb->nob_x = clamp(mp.x, sb->start_x, sb->end_x);

	return 1;
}
