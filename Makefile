fraktalismus: fraktalismus.c
	gcc -O3 -Wall -o fraktalismus fraktalismus.c -lm `pkg-config --cflags --libs sdl2 --libs cairo` `cups-config --cflags --libs`
