//
// fraktalismus.c - Written by Ted Burke - last updated 12-July-2017
//
// To compile:
//
//   gcc -Wall -o fraktalismus fraktalismus.c -lm `pkg-config --cflags --libs sdl2 --libs cairo` `cups-config --cflags --libs`
//
// If necessary, install these libraries:
//
//   apt install libsdl2-dev libcups2-dev libcairo2-dev
//

#include <stdio.h>      // For file i/o, debug printing, etc.
#include <complex.h>    // Complex arithmetic for fractal generation
#include <SDL.h>        // For screen rendering
#include <time.h>       // For naming print files with current time
#include <cairo.h>      // For generating print images
#include <cairo-ps.h>   // Postscript printing
#include <cups/cups.h>  // Printer access

// Fractal image height and width
#define H 1080
#define W 1920
//#define H 540
//#define W 960

// Fractal image pixels (ARGB)
Uint32 p[H*W];

// Template images (including height and width definitions)
// Two simple drawn shapes that seed the fractal creation
#define TH 1024
#define TW 1024
unsigned char template[2][TH][TW];

// A4 width, height in points, from GhostView manual:
#define PAGEWIDTH  595
#define PAGEHEIGHT 842

// Function prototypes
void print_card(int print_hard_copy);
void generate_fractal(Uint32 *p, int w, int h, double px, complex double centre, int fmode, complex double c1, complex double c2, int cmode, int invert_colour, int reverse_template);

int main(int argc, char *argv[])
{
	//int m, n, x, y;
	//complex double c1, c2, z, zz, zzz;
	//Uint32 tx, ty, tn, txx, tyy;
	//unsigned char r, g, b, a;
	
	int n, m;
	complex double c1, c2;
	
	double scaling_factor = 0.005;
	int invert_colour = 0;
	int reverse_template = 0;
	int colour_mode = 0, colour_modes = 4;
	int function_mode = 0, function_modes = 5;

	int exiting = 0;
	char buffer[1024];
	
	// Load template images from file
	FILE* f;
	for (n=0 ; n<2 ; ++n)
	{
		f = fopen(argv[n+1], "r");
		if (f == NULL)
		{
			fprintf(stderr, "Error opening template image file\n");
			exit(1);
		}
		for (m=0 ; m<3 ; ++m)
		{
			fscanf(f, "%[^\n]\n", buffer);
			if (buffer[0]=='#') m--;
		}
		fread(template[n], TW, TH, f);
		fclose(f);
	}
	
	// Initialise SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
	{
		fprintf(stderr, "Failed to initialise SDL\n");
		exit(1);
	}
	
	// Set up display. If a second screen is attached, use that.
	// Otherwise, use the main screen.
	int display_number = 0;
	if (SDL_GetNumVideoDisplays() > 1) display_number = 1;
	SDL_Rect displayRect;
	SDL_GetDisplayBounds(display_number, &displayRect);
	printf("Display %d: x=%d, y=%d, w=%d, h=%d\n", display_number, displayRect.x, displayRect.y, displayRect.w, displayRect.h);
	
	// Create SDL window and renderer
	SDL_Window *sdlWindow;
	SDL_Renderer *sdlRenderer;
	sdlWindow = SDL_CreateWindow("Fraktalismus Window", displayRect.x, displayRect.y, displayRect.w, displayRect.h, SDL_WINDOW_FULLSCREEN_DESKTOP);
	sdlRenderer = SDL_CreateRenderer(sdlWindow, 0, 0);
	
	// Clear the new window
	SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderPresent(sdlRenderer);
	
	// Create texture for frame
	SDL_Texture *sdlTexture;
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
	
	// Initialise time for measuring duration of frames
	Uint32 lastTime, currentTime;
	lastTime = SDL_GetTicks();
	
	SDL_Event event;
	while (!exiting)
	{
		// Process any pending user input events
		while (!exiting && SDL_PollEvent(&event))
		{
			if (event.type == SDL_KEYDOWN)
			{
				if (event.key.keysym.sym == SDLK_UP) scaling_factor /= 1.1;
				else if (event.key.keysym.sym == SDLK_DOWN) scaling_factor *= 1.1;
				else if (event.key.keysym.sym == SDLK_ESCAPE) exiting = 1;
				else if (event.key.keysym.sym == SDLK_p) print_card(1); // Print hard copy
				else if (event.key.keysym.sym == SDLK_s) print_card(0); // Only print to file
				else if (event.key.keysym.sym == SDLK_i) invert_colour = 1 - invert_colour;
				else if (event.key.keysym.sym == SDLK_r) reverse_template = 1 - reverse_template;
				else if (event.key.keysym.sym == SDLK_q) exiting = 1;
				else if (event.key.keysym.sym == SDLK_c) {colour_mode = (colour_mode + 1) % colour_modes; printf("Colour mode: %d\n", colour_mode);}
				else if (event.key.keysym.sym == SDLK_f) {function_mode = (function_mode + 1) % function_modes; printf("Function mode: %d\n", function_mode);}
			}
		}
		
		// Get mouse position
		int x, y;
		SDL_GetMouseState(&x, &y);
		c1 = scaling_factor * ((x - W/2) + (y - H/2)*I);
		c2 = -0.625 - 0.4*I;
		
		// Generate fractal
		generate_fractal(p, W, H, scaling_factor, 0, function_mode, c1, c2, colour_mode, invert_colour, reverse_template);
		
		// Draw fractal image
		SDL_UpdateTexture(sdlTexture, NULL, p, W * sizeof(Uint32));
		SDL_RenderClear(sdlRenderer);
		SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
		SDL_RenderPresent(sdlRenderer);
		
		// Measure frame time
		currentTime = SDL_GetTicks();
		printf("Frame time: %d ms\n", currentTime - lastTime);
		lastTime = currentTime;
	}
	
	// Destroy SDL objects
    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(sdlWindow);

	// Close SDL
	SDL_Quit();
	
	return 0;
}

void generate_fractal(Uint32 *p, int w, int h, double px, complex double centre, int fmode, complex double c1, complex double c2, int cmode, int invert_colour, int reverse_template)
{
	int n, x, y;
	complex double z, zz; //, zzz;
	Uint32 tx, ty, tn, txx, tyy;
	unsigned char r, g, b, a;

	Uint32 start_time, timeout = 5000;
	start_time = SDL_GetTicks();
	
	// Generate fractal image
	for (y=0 ; y<=H/2 ; ++y) for (x=0 ; x<W ; ++x)
	{
		if ((SDL_GetTicks() - start_time) > timeout) return; //{x=W; y=H;}
		
		z = px * ((x-W/2) + I*(y-H/2));
		
		for (n=0 ; n<25 ; ++n)
		{
			// Find template coordinates
			txx = TW*(0.5 + 0.25*creal(z));
			tx = txx & 1023;
			tyy = TH*(0.5 + 0.25*cimag(z));
			ty = tyy & 1023;
			tn = ((txx >> 10)+(tyy >> 10))%2;
			if ((tx!=txx || ty!=tyy) && n > 1)
			{
				if (reverse_template == 1 && template[tn][ty][tx] > 80) break;
				if (reverse_template == 0 && template[tn][ty][tx] < 80) break;
			}
			
			// Iterate z
			if (fmode == 0)
			{
				zz = z*z;
				z = (zz + c1)/(zz);
			}
			else if (fmode == 1)
			{
				zz = z*z;
				z = (zz - c1)/(zz - c2);
			}
			else if (fmode == 2)
			{
				zz = z*z;
				z = (zz + c1)/csin(zz); // good like carpet
			}
			else if (fmode == 3)
			{
				zz = z*z;
				z = (zz + c1)/(zz*c1); // GOOD BLOBS / DUST!!!
			}
			else if (fmode == 4)
			{
				zz = z*z;
				z = (zz + c1); // classic
			}
		}
		
		// Colour mapping
		a = 255;
		r = g = b = 0;
		
		if (cmode == 0)
		{
			r = g = b = 10*n;
			
			//double angle = 2.0*M_PI*n/25.0;
			r = g = b = 255;
			if ((n+0)%6 < 3) r = 10*n;
			if ((n+2)%6 < 3) g = 10*n;
			if ((n+4)%6 < 3) b = 10*n;
		}
		else if (cmode == 1)
		{
			r = g = b = 255;
			if (n%2) r = 10*n;
			else g = 10*n;
		}
		else if (cmode == 2)
		{
			r = g = b = 255;
			if (n%2) r = g = 10*n;
			else b = 10*n;
		}
		else if (cmode == 3)
		{
			r = g = b = 10*n;
		}
		
		// Invert colour if selected
		if (invert_colour)
		{
			r = 255 - r;
			g = 255 - g;
			b = 255 - b;
		}
		p[y*W+x] = (a<<24)+(r<<16)+(g<<8)+b;
		if (y>0) p[(H-y)*W+(W-1-x)] = p[y*W+x];
	}
}

void print_card(int print_hard_copy)
{
	// Render date and time as string for filename(s)
    char filename[1024];
    time_t rawtime;
    struct tm t;
    time(&rawtime);
    localtime_r(&rawtime, &t);
    sprintf(filename, "prints/%04d_%02d_%02d_%02d-%02d-%02d.ps",
				1900 + t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    fprintf(stderr, "%s\n", filename);
	
	// Create Cairo surface and context
	cairo_surface_t* surface = cairo_ps_surface_create(filename, PAGEWIDTH, PAGEHEIGHT);
	cairo_t *cr = cairo_create(surface);

	// Copy fractal image to cairo surface
	int rowstride, y;
	cairo_surface_t *fractal_surface;
	fractal_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, W, H);
	rowstride = cairo_image_surface_get_stride (fractal_surface);
	unsigned char *surface_pixels = cairo_image_surface_get_data (fractal_surface);
	for (y=0 ; y<H ; ++y) memcpy(surface_pixels+(y*rowstride), p+(y*W), W*4);
	cairo_surface_mark_dirty (fractal_surface);

	double margin = 20.0;
	cairo_rectangle(cr, margin, margin, PAGEWIDTH-2*margin, 0.5*PAGEHEIGHT - 2*margin);
	cairo_clip(cr);
	cairo_new_path(cr); // path not consumed by clip
	cairo_translate(cr, PAGEWIDTH/2, PAGEHEIGHT/4);
	cairo_scale(cr, (0.5*PAGEHEIGHT-2.0*margin)/H, (0.5*PAGEHEIGHT-2.0*margin)/H);
	cairo_translate(cr, -W/2, -H/2);
	cairo_set_source_surface (cr, fractal_surface, 0, 0);
	cairo_paint (cr);
	
	// Draw frame
	cairo_identity_matrix (cr);
	cairo_reset_clip(cr);
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 3);
	cairo_rectangle(cr, margin, margin, PAGEWIDTH-2*margin, 0.5*PAGEHEIGHT - 2*margin);
	cairo_stroke(cr);
	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_set_line_width(cr, 1);
	cairo_rectangle(cr, margin, margin, PAGEWIDTH-2*margin, 0.5*PAGEHEIGHT - 2*margin);
	cairo_stroke(cr);

	// Finish drawing and destroy Cairo surface and context
	cairo_show_page(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(fractal_surface);
	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);

	// Print the file
	if (print_hard_copy) cupsPrintFile(cupsGetDefault(), filename, "cairo PS", 0, NULL);
	
	// Make a copy of the output file for debugging
	char command[1024];
	sprintf(command, "cp %s aaa.ps", filename);
	system(command);
}
