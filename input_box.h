#ifndef INPUT_BOX
#define INPUT_BOX

#include <raylib.h>

#define MAX_TEXT_SIZE 20

typedef struct input_box
{
    char* label;

    char text[MAX_TEXT_SIZE];
    int text_size;

    int focused;

    Rectangle rect;
} input_box;

input_box
ib_init(char* label, int x, int y, char* init_text);

void
ib_draw(input_box* ib);

int
ib_get_text_as_integer(input_box* ib);

float
ib_get_text_as_float(input_box* ib);

char*
ib_get_text_as_string(input_box* ib);

// if we get some input and modify the text
// return 1 to indicate so, else 0
int
ib_get_input(input_box* ib);

void
ib_check_focus(input_box* ib);

#endif
