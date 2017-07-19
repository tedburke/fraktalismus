//
// camtest.c - written by Ted Burke - 14-7-2017
//
// To compile:
//
//    gcc -o camtest camtest.c -lm `pkg-config --cflags --libs sdl2`
//    gcc -O3 -Wall -o camtest camtest.c -lm `pkg-config --cflags --libs sdl2`
//

#include <stdio.h>
#include <SDL.h>

#define W 640
#define H 480

#define HW 640
#define HH 300

//Uint32 p[H*W/2];
unsigned char p[H][W][2];
unsigned char q[H][W][2];
unsigned char h[H][W][4];

int histogram[3][256] = {0};
int hist_max = 0;
void rebuild_histogram();

int left=W/4, right=3*W/4, top=H/4, bottom=3*H/4;

int main()
{
    int n=0, exiting=0, threshold=127, byte_count, x, y;

	// Initialise SDL
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_Window *sdlWindow = SDL_CreateWindow("Camtest", 120, 100, W, 2*H, 0);
	SDL_Renderer *sdlRenderer = SDL_CreateRenderer(sdlWindow, 0, 0);
	SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderPresent(sdlRenderer);
	
	SDL_Texture *video_texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_YUY2, SDL_TEXTUREACCESS_STREAMING, W, H);
	SDL_Texture *histogram_texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
	
	SDL_Event event;
	
	// Define regions within window for video and histogram
	SDL_Rect video_rect;
	video_rect.x = 0;
	video_rect.y = 0;
	video_rect.w = W;
	video_rect.h = H;
	
	SDL_Rect histogram_rect;
	histogram_rect.x = 0;
	histogram_rect.y = H;
	histogram_rect.w = W;
	histogram_rect.h = H;
		
    // Open camera via pipe
    fprintf(stderr, "Opening camera via pipe\n");
    //FILE *cam = popen("ffmpeg -i /dev/video1 -f image2pipe -vcodec rawvideo -pix_fmt rgb24 - < /dev/null", "r");
    FILE *cam = popen("ffmpeg -i /dev/video1 -f image2pipe -vcodec rawvideo - < /dev/null", "r");
    
	while (!exiting)
	{
		// Process any pending user input events
		while (!exiting && SDL_PollEvent(&event))
		{
			if (event.type == SDL_KEYDOWN)
			{
				if (event.key.keysym.sym == SDLK_UP) threshold += 5;
				else if (event.key.keysym.sym == SDLK_DOWN) threshold -= 5;
				else if (event.key.keysym.sym == SDLK_ESCAPE) exiting = 1;
				else if (event.key.keysym.sym == SDLK_q) exiting = 1;
				else if (event.key.keysym.sym == SDLK_s)
				{
					// Write last frame to file
					fprintf(stderr, "Writing frame to PPM file\n");
					FILE *f = fopen("snapshot.ppm", "w");
					fprintf(f, "P6\n%d %d\n255\n", W, H);
					fwrite(p, 3*640, 480, f);
					fclose(f);
				}
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN)
			{
				left = event.button.x;
				top = event.button.y;
			}
			else if (event.type == SDL_MOUSEBUTTONUP)
			{
				right = event.button.x;
				bottom = event.button.y;
			}
		}
		
        // Read a frame from the input pipe into the buffer
        byte_count = fread(p, 1, H*W*2, cam);
        if (byte_count != H*W*2)
        {
            fprintf(stderr, "Got wrong number of bytes from pipe!\n");
            return 1;
        }
        
        // Rebuild histogram
		rebuild_histogram();
        
        // Process this frame
        for (y=top ; y<bottom ; ++y) for (x=left ; x<right ; ++x)
        {
            if (abs(p[y][x][1] - 127) < 10 && p[y][x][0] > 100)
            {
				p[y][x][0] = 255;
				p[y][x][1] = 255;
			}
        }
        
        // Draw boundaries of analysis region
        for (y=0 ; y<H ; ++y)
        {
			p[y][left][0] = 255;
			p[y][left][1] = 127;
			p[y][right][0] = 255;
			p[y][right][1] = 127;
		}
		
        for (x=0 ; x<W ; ++x)
        {
			p[top][x][0] = 255;
			p[top][x][1] = 127;
			p[bottom][x][0] = 255;
			p[bottom][x][1] = 127;
		}
		
		// Draw frame
		SDL_UpdateTexture(video_texture, NULL, p, W*2);
		SDL_UpdateTexture(histogram_texture, NULL, h, W*4);
		SDL_RenderClear(sdlRenderer);
		SDL_RenderCopy(sdlRenderer, video_texture, NULL, &video_rect);
		SDL_RenderCopy(sdlRenderer, histogram_texture, NULL, &histogram_rect);
		SDL_RenderPresent(sdlRenderer);
    }

    // Close camera pipe
    fprintf(stderr, "Closing camera pipe\n");
    fflush(cam);
    fprintf(stderr, "ffmpeg pipe exit value is %d\n", pclose(cam));
        
	// Destroy SDL objects
    SDL_DestroyTexture(video_texture);
    SDL_DestroyTexture(histogram_texture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(sdlWindow);

	// Close SDL
	SDL_Quit();
	
    return 0;
}

void rebuild_histogram()
{
	int n, m, x, y;
	
	// Regenerate histogram data
	for (n=0 ; n<3 ; ++n) for (m=0 ; m<256 ; ++m) histogram[n][m] = 0;	
	for (y=top ; y<bottom ; ++y) for (x=left ; x<right ; ++x) 
	{
		histogram[0][p[y][x][0]]++;
		histogram[1+(x%2)][p[y][x][1]]++;
	}
	
	// Find max value in any histogram
	hist_max = 0;
	for (m=0 ; m<256 ; ++m)
	{
		if (histogram[0][m] > hist_max) hist_max = histogram[0][m];
		if (histogram[1][m] > hist_max) hist_max = histogram[1][m];
		if (histogram[2][m] > hist_max) hist_max = histogram[2][m];
	}
	
	// Clear histogram canvas
	memset(h,255,H*W*4);
	
	// Redraw histogram on canvas
	for (m=0 ; m<256 ; ++m)
	{
		for (n=150-((120*histogram[0][m])/hist_max) ; n<150 ; ++n) h[n][m][1] = h[n][m][2] = h[n][m][3] = 0;
		for (n=300-((120*histogram[1][m])/hist_max) ; n<300 ; ++n) h[n][m][2] = h[n][m][3] = 0;
		for (n=450-((120*histogram[2][m])/hist_max) ; n<450 ; ++n) h[n][m][1] = h[n][m][2] = 0;
	}
}

