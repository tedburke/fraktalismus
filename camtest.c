//
// camtest.c - written by Ted Burke - 14-7-2017
//
// To compile:
//
//    gcc -o camtest camtest.c
//

#include <stdio.h>

#define H 640
#define W 480

unsigned char p[H][W][3];

int main()
{
    int n, count;
    
    // Open camera via pipe
    fprintf(stderr, "Opening camera via pipe\n");
    FILE *cam = popen("ffmpeg -i /dev/video1 -f image2pipe -vcodec rawvideo -pix_fmt rgb24 - < /dev/null", "r");
    
    for(n=0 ; n<30 ; ++n)
    {
        // Read a frame from the input pipe into the buffer
        count = fread(p, 1, H*W*3, cam);
         
        // If we didn't get a frame of video, we're probably at the end
        if (count != H*W*3)
        {
            fprintf(stderr, "Got wrong number of bytes from pipe!\n");
            return 1;
        }
        
        // Process this frame
        /*
        for (y=0 ; y<H ; ++y) for (x=0 ; x<W ; ++x)
        {
            // Invert each colour component in every pixel
            frame[y][x][0] = 255 - frame[y][x][0]; // red
            frame[y][x][1] = 255 - frame[y][x][1]; // green
            frame[y][x][2] = 255 - frame[y][x][2]; // blue
        }
        */
        
        printf ("Frame %d\n", n);
    }

    // Close camera pipe
    fprintf(stderr, "Closing camera pipe\n");
    fflush(cam);
    fprintf(stderr, "Pipe exit value is %d\n", pclose(cam));
    
    // Write last frame to file
    fprintf(stderr, "Writing frame to PPM file\n");
    FILE *f = fopen("snapshot.ppm", "w");
    fprintf(f, "P6\n640 480\n255\n");
    fwrite(p, 3*640, 480, f);
    fclose(f);
    
    return 0;
}
