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
// Additional notes:
//
//   apt install pstoedit (for LaTeX rendering in Inkscape)
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

// Fractal image pixels (ARGB)
Uint32 p[H*W];

// Template images (including height and width definitions)
// Two simple drawn shapes that seed the fractal creation
#define TH 1024
#define TW 1024
unsigned char template[2][TH][TW][4] = {0}; // ARGB pixels

// Function prototypes
void print_card(int print_hard_copy);

void generate_fractal (
		Uint32 *p, int w, int h, double px, complex double centre,
		complex double a, complex double b, complex double c, complex double d,
		int cmode, int invert_colour, int reverse_template );

void update_template(SDL_Renderer *sdlRenderer);

// Main function
int main(int argc, char *argv[])
{
	int n, m;
	complex double a, b, c, d;
	
	double scaling_factor = 0.005;
	int invert_colour = 0;
	int reverse_template = 0;
	int colour_mode = 0, colour_modes = 5;
	int function_mode = 0, function_modes = 4;

	int exiting = 0;
	char buffer[1024];
	
	// Load template images from file
	if (argc > 1)
	{
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
				else if (event.key.keysym.sym == SDLK_u) update_template(sdlRenderer);
			}
		}
		
		// Get mouse position
		int x, y;
		SDL_GetMouseState(&x, &y);
		a = b = c = d = 1;
		
		if (function_mode == 0)
		{
			a = 1; b = 1;
			c = scaling_factor * ((x - W/2) + (y - H/2)*I);
			d = 0;
		}
		else if (function_mode == 1)
		{
			a = 1; b = 1;
			c = scaling_factor * ((x - W/2) + (y - H/2)*I);
			d = -0.625 - 0.4*I;
		}
		else if (function_mode == 2)
		{
			a = 1;
			b = c = scaling_factor * ((x - W/2) + (y - H/2)*I);
			d = 0;
		}
		else if (function_mode == 3)
		{
			a = 1;
			b = 0;
			c = scaling_factor * ((x - W/2) + (y - H/2)*I);
			d = 1;
		}
		
		// Generate fractal
		generate_fractal(p, W, H, scaling_factor, 0, a, b, c, d, colour_mode, invert_colour, reverse_template);
		
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

void generate_fractal(Uint32 *p, int w, int h, double px, complex double centre, complex double a, complex double b, complex double c, complex double d, int cmode, int invert_colour, int reverse_template)
{
	int n, x, y;
	complex double z, zz; //, zzz;
	Uint32 tx, ty, tn, txx, tyy;
	unsigned char red, green, blue, alpha;

	Uint32 start_time, timeout = 5000;
	start_time = SDL_GetTicks();
	
	// Generate fractal image
	for (y=0 ; y<=H/2 ; ++y) for (x=0 ; x<W ; ++x)
	{
		if ((SDL_GetTicks() - start_time) > timeout) return;
		
		z = px * ((x-w/2) + I*(y-h/2));
		
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
				if (reverse_template == 1 && template[tn][ty][tx][0] > 127) break;
				if (reverse_template == 0 && template[tn][ty][tx][0] < 127) break;
			}
			
			// Iterate z
			zz = z*z;
			z = (a*zz + c)/(b*zz + d);
		}
		
		// Colour mapping
		alpha = 255;
		red = green = blue = 0;
		
		if (cmode == 0)
		{
			red = green = blue = 255;
			if ((n+0)%6 < 3) red = 10*n;
			if ((n+2)%6 < 3) green = 10*n;
			if ((n+4)%6 < 3) blue = 10*n;
		}
		else if (cmode == 1)
		{
			red = green = blue = 255;
			if (n%2) red = 10*n;
			else green = 10*n;
		}
		else if (cmode == 2)
		{
			red = green = blue = 255;
			if (n%2) red = green = 10*n;
			else blue = 10*n;
		}
		else if (cmode == 3)
		{
			red = green = blue = 10*n;
		}
		else if (cmode == 4)
		{
			red   = template[tn][ty][tx][1] + (n/25.0)*(255-template[tn][ty][tx][2]);
			green = template[tn][ty][tx][2] + (n/25.0)*(255-template[tn][ty][tx][1]);
			blue  = template[tn][ty][tx][3] + (n/25.0)*(255-template[tn][ty][tx][0]);
		}
		
		// Invert colour if selected
		if (invert_colour)
		{
			red = 255 - red;
			green = 255 - green;
			blue = 255 - blue;
		}

		p[y*W+x] = (alpha<<24)+(red<<16)+(green<<8)+blue;
		p[y*W+x] = (alpha<<24)+(red<<16)+(green<<8)+blue;
		
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
	
	// A4 width, height in points, from GhostView manual:
	const double page_width = 595;
	const double page_height = 842;
	const double page_margin = 20.0;

	// Create Cairo surface and context
	cairo_surface_t* ps_surface = cairo_ps_surface_create(filename, page_width, page_height);
	cairo_t *cr = cairo_create(ps_surface);

	// Copy fractal image to cairo surface
	int rowstride, y;
	cairo_surface_t *fractal_surface;
	fractal_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, W, H);
	rowstride = cairo_image_surface_get_stride (fractal_surface);
	unsigned char *surface_pixels = cairo_image_surface_get_data (fractal_surface);
	for (y=0 ; y<H ; ++y) memcpy(surface_pixels+(y*rowstride), p+(y*W), W*4);
	cairo_surface_mark_dirty (fractal_surface);

	cairo_rectangle(cr, page_margin, page_margin, page_width-2.0*page_margin, 0.5*page_height - 2*page_margin);
	cairo_clip(cr);
	cairo_new_path(cr); // path not consumed by clip
	cairo_translate(cr, page_width/2.0, page_height/4.0);
	cairo_scale(cr, (0.5*page_height-2.0*page_margin)/H, (0.5*page_height-2.0*page_margin)/H);
	cairo_translate(cr, -W/2, -H/2);
	cairo_set_source_surface (cr, fractal_surface, 0, 0);
	cairo_paint (cr);
	
	// Draw frame
	const int draw_frame = 0;
	if (draw_frame)
	{
		cairo_identity_matrix (cr);
		cairo_reset_clip(cr);
		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
		cairo_set_line_width(cr, 3.0);
		cairo_rectangle(cr, page_margin, page_margin, page_width-2.0*page_margin, 0.5*page_height - 2.0*page_margin);
		cairo_stroke(cr);
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_set_line_width(cr, 1.0);
		cairo_rectangle(cr, page_margin, page_margin, page_width-2.0*page_margin, 0.5*page_height - 2.0*page_margin);
		cairo_stroke(cr);
	}

	// Draw back of card
	cairo_identity_matrix (cr);
	cairo_reset_clip (cr);
	cairo_surface_t *rear_surface = cairo_image_surface_create_from_png ("cardback.png");
	double png_height = cairo_image_surface_get_height (rear_surface);
	double png_width = cairo_image_surface_get_width (rear_surface);
	fprintf(stderr, "PNG width and height: %lf x %lf\n", png_width, png_height);
	cairo_scale (cr, page_width / png_width, page_width / png_width);
	cairo_translate (cr, 0, png_height);
	cairo_set_source_surface (cr, rear_surface, 0, 0);
	cairo_paint (cr);
	cairo_surface_destroy(rear_surface);
	
	// Draw templates on back of card
	int template_size = 60;	
	
	// Template 0
	cairo_surface_t *template_surface0;	
	template_surface0 = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, TW, TH);
	rowstride = cairo_image_surface_get_stride (template_surface0);
	surface_pixels = cairo_image_surface_get_data (template_surface0);
	for (y=0 ; y<TH ; ++y) memcpy(surface_pixels+(y*rowstride), &(template[0][y][0][0]), TW*4);
	cairo_surface_mark_dirty (template_surface0);
	cairo_identity_matrix (cr);
	cairo_reset_clip (cr);
	cairo_translate(cr, page_margin, page_height-page_margin-template_size);
	cairo_scale(cr, template_size*1.0/TW, template_size*1.0/TH);
	cairo_set_source_surface (cr, template_surface0, 0, 0);
	cairo_paint (cr);
	
	// Template 1
	cairo_surface_t *template_surface1;	
	template_surface1 = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, TW, TH);
	rowstride = cairo_image_surface_get_stride (template_surface1);
	surface_pixels = cairo_image_surface_get_data (template_surface1);
	for (y=0 ; y<TH ; ++y) memcpy(surface_pixels+(y*rowstride), &(template[1][y][0][0]), TW*4);
	cairo_surface_mark_dirty (template_surface1);
	cairo_identity_matrix (cr);
	cairo_reset_clip (cr);
	cairo_translate(cr, page_width-page_margin-template_size, page_height-page_margin-template_size);
	cairo_scale(cr, template_size*1.0/TW, template_size*1.0/TH);
	cairo_set_source_surface (cr, template_surface1, 0, 0);
	cairo_paint (cr);

	// Finish drawing and destroy Cairo surface and context
	cairo_show_page(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(template_surface0);
	cairo_surface_destroy(template_surface1);
	cairo_surface_destroy(fractal_surface);
	cairo_surface_flush(ps_surface);
	cairo_surface_destroy(ps_surface);

	// Print the file
	if (print_hard_copy) cupsPrintFile(cupsGetDefault(), filename, "cairo PS", 0, NULL);
	
	// Make a copy of the output file for debugging
	char command[1024];
	sprintf(command, "cp %s aaa.ps", filename);
	system(command);
}

// Video frame pixels
#define vw 1280
#define vh 720
unsigned char vp[vh][vw][2];

void update_template(SDL_Renderer *sdlRenderer)
{
	int mouse_x=0, mouse_y=0;
	int x1, y1, x2, y2, left, right, top, bottom;
	x1 = left = vw/4;
	x2 = right = 3*vw/4;
	y1 = top = vh/4;
	y2 = bottom = 3*vh/4;
	
	static int tol = 20; // colour matching tolerance

	int n = -1; // number of template to update (will be set to 0 or 1 if key is pressed)
    int exiting_video=0;
    int byte_count;
    int vx, vy, tx, ty;

	// Open SDL window for video
	SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderPresent(sdlRenderer);
	
	SDL_Texture *video_texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_YUY2, SDL_TEXTUREACCESS_STREAMING, vw, vh);
	
	SDL_Event event;
	
	// Define regions within window for video and histogram
	SDL_Rect video_rect;
	video_rect.x = 0;
	video_rect.y = 0;
	video_rect.w = vw;
	video_rect.h = vh;
	
    // Open camera via pipe
    fprintf(stderr, "Opening camera via pipe\n");
    char pipe_command[1024];
    sprintf(pipe_command, "ffmpeg -video_size %dx%d -i /dev/video1 -f image2pipe -vcodec rawvideo - < /dev/null", vw, vh);
    fprintf(stderr, "Opening pipe:\n%s\n", pipe_command);
    FILE *cam = popen(pipe_command, "r");
    
	while (!exiting_video)
	{
		// Process any pending user input events
		while (!exiting_video && SDL_PollEvent(&event))
		{
			if (event.type == SDL_KEYDOWN)
			{
				if (event.key.keysym.sym == SDLK_UP) tol += 5;
				else if (event.key.keysym.sym == SDLK_DOWN) tol -= 5;
				else if (event.key.keysym.sym == SDLK_ESCAPE) exiting_video = 1;
				else if (event.key.keysym.sym == SDLK_q) exiting_video = 1;
				else if (event.key.keysym.sym == SDLK_1) n = 0;
				else if (event.key.keysym.sym == SDLK_2) n = 1;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN)
			{
				x1 = event.button.x;
				y1 = event.button.y;
			}
			else if (event.type == SDL_MOUSEMOTION)
			{
				mouse_x = event.button.x;
				mouse_y = event.button.y;
				
				if (event.motion.state & SDL_BUTTON_LMASK)
				{
					x2 = mouse_x;
					y2 = mouse_y;
				}
			}
			
			// Set boundaries
			left   = x1 < x2 ? x1 : x2;
			right  = x1 < x2 ? x2 : x1;
			top    = y1 < y2 ? y1 : y2;
			bottom = y1 < y2 ? y2 : y1;
		}
		
        // Read a YUY2 frame from the input pipe into the buffer
        byte_count = fread(vp, 1, vh*vw*2, cam);
        if (byte_count != vh*vw*2)
        {
            fprintf(stderr, "Got wrong number of bytes from pipe. Exiting video...\n");
            return;
        }
        
        // Scan edge pixels to identify background colour range
        int Y, U, V, red, green, blue;
        int Ymin=255, Ymax=0;
        int Umin=255, Umax=0;
        int Vmin=255, Vmax=0;
        
        for (vy=top ; vy<bottom ; ++vy) for (vx=left ; vx<right ; ++vx)
        {
			if (vx==left+2 && vy>=top+2 && vy<bottom-2) vx = right-2;
			
			Y = vp[vy][vx][0];
			U = (vx%2) ? vp[vy][vx-1][1] : vp[vy][vx][1];
			V = (vx%2) ? vp[vy][vx][1] : vp[vy][vx+1][1];
			
			if (Y<Ymin) Ymin = Y;
			if (Y>Ymax) Ymax = Y;
			if (U<Umin) Umin = U;
			if (U>Umax) Umax = U;
			if (V<Vmin) Vmin = V;
			if (V>Vmax) Vmax = V;
		}

        Ymin-=tol; Ymax+=tol;
        Umin-=tol; Umax+=tol;
        Vmin-=tol; Vmax+=tol;
		
        // Update template if "1" or "2" was pressed
        if (n>=0)
        {
			fprintf(stderr, "Updating template %d\n", n);
			
			for (ty=0 ; ty<TH ; ++ty) for (tx=0 ; tx<TW ; ++tx)
			{
				vx = left + (tx*1.0/TW)*(right-left);
				vy = top + (ty*1.0/TH)*(bottom-top);
				
				Y = vp[vy][vx][0];
				U = (vx%2) ? vp[vy][vx-1][1] : vp[vy][vx][1];
				V = (vx%2) ? vp[vy][vx][1] : vp[vy][vx+1][1];
				
				blue = 1.164*(Y-16) + 2.018*(U-128);
				if (blue < 0) blue = 0;
				if (blue > 255) blue = 255;
				green = 1.164*(Y-16) - 0.813*(V-128) - 0.391*(U-128);
				if (green < 0) green = 0;
				if (green > 255) green = 255;
				red = 1.164*(Y-16) + 1.596*(V-128);
				if (red < 0) red = 0;
				if (red > 255) red = 255;
				
				template[n][ty][tx][1] = red;
				template[n][ty][tx][2] = green;
				template[n][ty][tx][3] = blue;
				
				if (Y>Ymin && Y<Ymax && U>Umin && U<Umax && V>Vmin && V<Vmax)
				{
					template[n][ty][tx][0] = 255;
				}
				else
				{
					template[n][ty][tx][0] = 0;
				}
			}
				
			n = -1; // reset flag
		}
		
        // Process this frame
        for (vy=top ; vy<bottom ; ++vy) for (vx=left ; vx<right ; ++vx)
        {
			Y = vp[vy][vx][0];
			U = (vx%2) ? vp[vy][vx-1][1] : vp[vy][vx][1];
			V = (vx%2) ? vp[vy][vx][1] : vp[vy][vx+1][1];
			
            if (Y>Ymin && Y<Ymax && U>Umin && U<Umax && V>Vmin && V<Vmax)
            {
				vp[vy][vx][0] = 255;
				vp[vy][vx][1] = 255;
			}
			else
            {
				vp[vy][vx][0] = 0;
				vp[vy][vx][1] = 0;
			}
        }
        
        // Draw mouse crosshairs and boundaries of analysis region
        for (vy=0 ; vy<vh ; ++vy)
        {
			// crosshairs
			vp[vy][mouse_x][0] = 0;
			vp[vy][mouse_x][1] = 127;
			
			// left and right boundaries of analysis region
			vp[vy][left][0] = 255;
			vp[vy][left][1] = 127;
			vp[vy][right][0] = 255;
			vp[vy][right][1] = 127;
		}
		
        for (vx=0 ; vx<vw ; ++vx)
        {
			// crosshairs
			vp[mouse_y][vx][0] = 0;
			vp[mouse_y][vx][1] = 127;
			
			// top and bottom boundaries of analysis region
			vp[top][vx][0] = 255;
			vp[top][vx][1] = 127;
			vp[bottom][vx][0] = 255;
			vp[bottom][vx][1] = 127;
		}
		
		// Draw frame
		SDL_UpdateTexture(video_texture, NULL, vp, vw*2);
		SDL_RenderClear(sdlRenderer);
		SDL_RenderCopy(sdlRenderer, video_texture, NULL, &video_rect);
		SDL_RenderPresent(sdlRenderer);
    }
	
    // Close camera pipe
    fprintf(stderr, "Closing camera pipe\n");
    fflush(cam);
    fprintf(stderr, "ffmpeg pipe exit value is %d\n", pclose(cam));
    
	// Destroy video-related SDL objects
    SDL_DestroyTexture(video_texture);
}
