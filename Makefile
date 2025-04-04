.PHONY: all

CC = gcc
LDFLAGS = -lopenal -lraylib -lm

SRC = main.c chuck_fft.c input_box.c button.c slide_bar.c util.c
OUT = auvi

all:
	$(CC) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)
