fraktalismus: fraktalismus.c
	gcc -Wall -o fraktalismus fraktalismus.c -lm `pkg-config --cflags --libs sdl2 --libs cairo` `cups-config --cflags --libs`
