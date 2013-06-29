#include <stdlib.h>
#include <memory.h>
#include <math.h>

#include <SDL/SDL.h>

#include "emscripten.h"	

// positive random number clipped to n
#define RAND(n) (abs(rand() % n))

// interpolate `a`, within the range of [x, y], between the values of [u, v]
#define LERP(a, x, y, u, v) ((a - x) * (v - u) / (y - x) + u)

struct vector
{
   int x, y;
};

struct color
{
	unsigned char r, g, b;
};

class BlobPair
{
public:
	BlobPair ();
	void render ();
	void physics ();

	vector pos [2];
	vector vel [2];
	vector accel[2];

	vector pair_vel;

	int size;
	unsigned char *blob_map;
	unsigned char *layer;

private:
	static const int VEL = 17;
	static const int PAIR_VEL = 15;
	static const int ACCEL = 5;
	static const int ATTRACT = 5;
};

class LavaLamp
{
public:
	LavaLamp ();
	~LavaLamp ();
	void update ();

	static const int NUM_PAIRS = 10;
	static const int NUM_LAYERS = 2;

	static const int SLOPE = 6;
	static const int DROP_OFF = 15;

  	static const int WIDTH = 640;
	static const int HEIGHT = 400;

	static const int START_WIDTH = WIDTH * 56 / 640;
	static const int BEND_BACK = HEIGHT * 324 / 400;
	static const int LAMP_TOP = HEIGHT * 22 / 400;
	static const int LAMP_BOTTOM = HEIGHT * 370 / 400;
	static const int SHADE_OFF = WIDTH * 2 + 1;

	static const int NUM_BLOB_SIZES = 3;
	static const int BLOB_SIZES[];

private:
	unsigned int frame_counter;
	
	int add_table[WIDTH];
	int left_table[HEIGHT];
	int right_table[HEIGHT];
	int span_table[HEIGHT];
	int lamp_x, lamp_width;

	unsigned char *buffer;
	unsigned char *background;
	unsigned char *blob_maps[NUM_BLOB_SIZES];
	unsigned char *layers[NUM_LAYERS];
	unsigned char pal[768];
	unsigned int pal32[256];

	BlobPair *blobs;

	SDL_Surface *screen;
};

const int LavaLamp::BLOB_SIZES[] = { 
	110 * LavaLamp::WIDTH / 640, 
	90 * LavaLamp::WIDTH / 640, 
	180 * LavaLamp::WIDTH / 640 
};

LavaLamp *lavaLamp;

/***** Main *****/

void loop ()
{
	lavaLamp->update();
}

int main(int argc, char** argv)
{
	lavaLamp = new LavaLamp();
	emscripten_set_main_loop(loop, 0, 0);
	return 1;
}

/***** Lavalamp manager *****/

LavaLamp::LavaLamp ()
{
	int p, cx;

	// Image buffers
	buffer = new unsigned char [WIDTH * HEIGHT];
	for (cx=0; cx < WIDTH * HEIGHT; cx++) buffer[cx] = 0;

	background = new unsigned char [WIDTH * HEIGHT];
	memset(background, 0, WIDTH * HEIGHT);

	for (cx=0; cx < NUM_LAYERS; cx++)
	{
		layers[cx] = new unsigned char [WIDTH * HEIGHT];
		memset(layers[cx], 10, WIDTH * HEIGHT);
	}

	// Generate the palette
	static const color yellow = { 180, 172, 48 };
	static const color dark_red = { 120, 20, 0 };
	static const color bright_red = { 255, 0, 0 };

	for (p=0; p < 10; p++)
	{
		pal[p*3] = 0;
		pal[p*3+1] = 0;
		pal[p*3+2] = 0;
	}

	for (p=10; p < 192; p++)
	{      
		pal[p*3] = LERP(p, 10, 192, yellow.r, dark_red.r);
		pal[p*3+1] = LERP(p, 10, 192, yellow.g, dark_red.g);
		pal[p*3+2] = LERP(p, 10, 192, yellow.b, dark_red.b);
	}

	for (p=192; p < 256; p++)
	{      
		pal[p*3] = LERP(p, 192, 256, dark_red.r, bright_red.r);
		pal[p*3+1] = LERP(p, 192, 256, dark_red.g, bright_red.g);
		pal[p*3+2] = LERP(p, 192, 256, dark_red.b, bright_red.b);
	}

	// 32-bit version of palette
	for (p=0; p < 256; p++)
	{
		pal32[p] = pal[p*3] + (pal[p*3+1] << 8) + (pal[p*3+2] << 16);
	}

	// Calculate blob maps
	float d;

	for (cx=0; cx < NUM_BLOB_SIZES; cx++)
	{
		blob_maps[cx] = new unsigned char [BLOB_SIZES[cx]*BLOB_SIZES[cx]];

		// Render circle to bitmap format
		for (int ry=0; ry < BLOB_SIZES[cx]; ry++)
		{
			for (int rx=0; rx < BLOB_SIZES[cx]; rx++)
			{
				d = (float)((ry-BLOB_SIZES[cx]/2) * (ry-BLOB_SIZES[cx]/2) +
				  (rx-BLOB_SIZES[cx]/2)*(rx-BLOB_SIZES[cx]/2));
  
				d = sqrt (d);

				if (d < (BLOB_SIZES[cx]/2))
				{
					if (((float)BLOB_SIZES[cx]/2-d) *
						((float)BLOB_SIZES[cx]/2-d) > 255)
						blob_maps[cx][ry*BLOB_SIZES[cx]+rx] = 255;
					else
						blob_maps[cx][ry*BLOB_SIZES[cx]+rx] =
							((float)BLOB_SIZES[cx]/2-d)*((float)BLOB_SIZES[cx]/2-d);
				}   
				else 
					blob_maps[cx][ry*BLOB_SIZES[cx]+rx] = 0;
			}
		}   
	}

	// Allocate blobs
	blobs = new BlobPair [NUM_PAIRS];

	// Set layers
	for (cx = 0; cx < NUM_PAIRS; cx++)
	{
		blobs[cx].layer = layers [cx % NUM_LAYERS];
	}

	// Set Sizes
	for (cx = 0; cx < (NUM_PAIRS-1)/3; cx++)
	{      
		blobs[cx].size = BLOB_SIZES[0];
		blobs[cx].blob_map = blob_maps[0];
	}
	for (cx = (NUM_PAIRS-1)/3; cx < NUM_PAIRS-1; cx++)
	{      
		blobs[cx].size = BLOB_SIZES[1];
		blobs[cx].blob_map = blob_maps[1];
	}

	// Special BIG BLOB at the bottom
	blobs[NUM_PAIRS-1].size = BLOB_SIZES[2];
	blobs[NUM_PAIRS-1].blob_map = blob_maps[2];

	blobs[NUM_PAIRS-1].layer = layers[0];

	blobs[NUM_PAIRS-1].pos[0].x = (WIDTH * 142 / 640) << 17;
	blobs[NUM_PAIRS-1].pos[0].y = (HEIGHT * 220 / 400) << 17;
	blobs[NUM_PAIRS-1].pos[1].x = (WIDTH * 173 / 640) << 17;
	blobs[NUM_PAIRS-1].pos[1].y = (HEIGHT * 220 / 400) << 17;

	blobs[NUM_PAIRS-1].pair_vel.x = 0;
	blobs[NUM_PAIRS-1].pair_vel.y = 0;
	blobs[NUM_PAIRS-1].vel[0].x = 0;
	blobs[NUM_PAIRS-1].vel[0].y = 0;
	blobs[NUM_PAIRS-1].vel[1].x = 0;
	blobs[NUM_PAIRS-1].vel[1].y = 0;

	blobs[NUM_PAIRS-1].accel[0].x = 0;
	blobs[NUM_PAIRS-1].accel[0].y = 0;
	blobs[NUM_PAIRS-1].accel[1].x = 0;
	blobs[NUM_PAIRS-1].accel[1].y = 0;

	int left, right;
	unsigned char bcolor;

	// Area containing the lamp image
	lamp_x = (WIDTH/2) - (START_WIDTH + BEND_BACK / SLOPE) - 3;
	lamp_width = ((START_WIDTH + BEND_BACK / SLOPE) << 1) + 6;

	// Useful lookup tables
	for (int tx=0; tx < WIDTH; tx++)
		add_table[tx] = (WIDTH/2 - tx) * (WIDTH/2 - tx);

	for (int ty=0; ty < HEIGHT; ty++)
	{
		if (ty < BEND_BACK)
		{
			left = WIDTH/2 - (START_WIDTH + ty / SLOPE);
			right = WIDTH/2 + START_WIDTH + ty / SLOPE;
		}
		else
		{
			left = WIDTH/2 - (START_WIDTH + (BEND_BACK/SLOPE) - (ty-BEND_BACK) / SLOPE);
			right = WIDTH/2 + (START_WIDTH + (BEND_BACK/SLOPE) - (ty-BEND_BACK) / SLOPE);
		}

		left_table[ty] = left;
		right_table[ty] = right;

		span_table[ty] = (right - left) >> 1;
		span_table[ty] *= span_table[ty];

		bcolor = (HEIGHT - 1 - ty)/7 + 80;

		for (int tx = left-1; tx < right+1; tx++)
		{
			background[ty * WIDTH + tx] = bcolor;
			background[ty * WIDTH + tx] += add_table[tx] * ((bcolor - 40) >>2 ) / span_table[ty];
		}
	}

	frame_counter = 0;

	SDL_Init(SDL_INIT_VIDEO);
	screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, SDL_SWSURFACE);
}

LavaLamp::~LavaLamp ()
{
	delete buffer;
	delete background;

	int cx;
	for (cx=0; cx < NUM_BLOB_SIZES; cx++)
	{
		delete blob_maps[cx];
	}

	for (cx=0; cx < NUM_LAYERS; cx++)
	{
		delete layers[cx];
	}
}

void LavaLamp::update ()
{
	int tx, ty, off, yoff, s, s_cur;

	// Clear layers
	for (int cx=0; cx < NUM_LAYERS; cx++)
	{
		for (ty = LAMP_TOP-2; ty < LAMP_BOTTOM+2; ty++)
			memset(layers[cx] + (ty * WIDTH) + lamp_x, 0, lamp_width);
	}      

	// Process blobs
	for (int cx=0; cx < NUM_PAIRS; cx++)
	{
		blobs[cx].physics();
		blobs[cx].render();
	}	

	//Shade / Render blobs into final buffer
	yoff = 0;
	for (ty=0; ty < HEIGHT; ty++)
	{
		if (ty <= LAMP_TOP)
		{
			off = yoff + left_table[ty];
			for (tx=0; tx < right_table[ty]-left_table[ty]; tx++)
				buffer[off+tx] = tx * 320 / WIDTH + 30;
		}
		else if (ty >= LAMP_BOTTOM)
		{
			off = yoff + left_table[ty];
			for (tx=0; tx < right_table[ty]-left_table[ty]; tx++)
				buffer[off+tx] = ((tx+(tx<<1))>>2) * 320 / WIDTH + 30;
		}

		else
		{        
			for (tx = left_table[ty]; tx < right_table[ty]; tx++)
			{
				off = yoff + tx;

				// Bottom layer
				s = layers[0][off - SHADE_OFF];
				s -= layers[0][off + SHADE_OFF];

				// Shade based on height
				s += (ty >> 1) + 127;

				// Curve shading some
				s += add_table[tx] * (s >> 3) / span_table[ty];

				s_cur = layers[0][off];

				s *= s_cur;
				s >>= 8;

				s += s_cur >> 3;

				if (s < 150)               
					s = background[off];
				else if (s > 255) s = 255;		// Clamp

				if (s > 240)
					s -= (tx & 3) ^ (ty & 3);	// Dithering

				buffer[off] = s;

				// Remaining Layers
				for (int cx=1; cx < NUM_LAYERS; cx++)
				{
					// Shading
					s = layers[cx][off - SHADE_OFF];
					s -= layers[cx][off + SHADE_OFF];

					// Shade based on height
					s += (ty >> 2) + 127;

					// Curve shading some
					s += add_table[tx] * (s >> 3) / span_table[ty];

					s_cur = layers[cx][off];

					s *= s_cur;
					s >>= 8;

					s += s_cur >> 3;

					s += cx << 3; // Extra shading based on layer

					if (s >= 168)
					{
						if (s > 255) s = 255; // Clamp
						buffer[off] = s;
					}
				}              
			}
		}

		if (frame_counter == 0)
		{
			unsigned char bcolor = (HEIGHT - 1 - ty)/SLOPE + 118;

			// Shade background
			for (tx=0; tx < left_table[ty]; tx++)
			buffer[yoff+tx] = bcolor - (tx * DROP_OFF / left_table[ty]);

			for (tx=right_table[ty]; tx < WIDTH; tx++)
			buffer[yoff+tx] = bcolor - ((tx-right_table[ty]) *
							 DROP_OFF / (right_table[ty]- WIDTH/2));
		}

		if (frame_counter < 16)
		{
			// Antialias edge of lamp
			off = yoff + left_table[ty];
			buffer[off-1] = (buffer[off-2]+buffer[off] + buffer[off - (WIDTH+1)]+buffer[off + (WIDTH-1)]) >> 2;

			off = yoff + right_table[ty];
			buffer[off] = (buffer[off+1]+buffer[off-1] + buffer[off - WIDTH]+buffer[off + WIDTH]) >> 2;
		}

		yoff += WIDTH;
	}

	// Map palette to 32-bits and blit
	SDL_LockSurface(screen);

	for (off=0; off < WIDTH * HEIGHT; off++)
	{
		*((Uint32*)screen->pixels + off) = pal32[buffer[off]];
	}

  	SDL_UnlockSurface(screen);
	
	frame_counter ++;
}

/***** Individual blobs (bound in pairs) *****/

BlobPair::BlobPair ()
{
   pos[0].x = RAND(LavaLamp::WIDTH) << 16;
   pos[0].y = RAND(LavaLamp::HEIGHT) << 16;
   pos[1].x = pos[0].x;
   pos[1].y = pos[0].y;
      
   vel[0].x = (RAND(1 << VEL) - (1 << (VEL-1))) >> 2;
   vel[0].y = RAND(1 << VEL) - (1 << (VEL-1));
   vel[1].x = (RAND(1 << VEL) - (1 << (VEL-1))) >> 2;
   vel[1].y = RAND(1 << VEL) - (1 << (VEL-1));
   pair_vel.x = (RAND(PAIR_VEL) - (1 << (PAIR_VEL-1))) >> 2;
   pair_vel.y = RAND(PAIR_VEL) - (1 << (PAIR_VEL-1));

   accel[0].x = (RAND(1 << ACCEL) - (1 << (ACCEL-1))) >> 2;
   accel[0].y = RAND(1 << ACCEL) - (1 << (ACCEL-1));
   accel[1].x = (RAND(1 << ACCEL) - (1 << (ACCEL-1))) >> 2;
   accel[1].y = RAND(1 << ACCEL) - (1 << (ACCEL-1));

   size = 0;
   blob_map = NULL;
   layer = NULL;
}

//Move the pair of blobs around
void BlobPair::physics (void)
{
   //Motion
   pos[0].x += vel[0].x + pair_vel.x;
   pos[0].y += vel[0].y + pair_vel.y;

   pos[1].x += vel[1].x + pair_vel.x;
   pos[1].y += vel[1].y + pair_vel.y;

   vel[0].x += accel[0].x;
   vel[0].y += accel[0].y;

   vel[1].x += accel[1].x;
   vel[1].y += accel[1].y;

   //Clip first in pair
   int lbound, rbound, bbound;
   int ty = pos[0].y >> 16;
   
   lbound = LavaLamp::WIDTH/2 - (LavaLamp::START_WIDTH + ty / LavaLamp::SLOPE) + 1;
   rbound = LavaLamp::WIDTH/2 + LavaLamp::START_WIDTH + ty / LavaLamp::SLOPE - 1;
   bbound = LavaLamp::HEIGHT * 1.25; //500;
   
   if ((pos[0].x>>16) + (size>>1) > rbound)
   {
      pos[0].x = (rbound - (size>>1)) << 16;
      vel[0].x = -vel[0].x;
      pair_vel.x = -pair_vel.x;
   }
   if ((pos[0].x>>16) - (size>>1) < lbound)
   {
      pos[0].x = (lbound + (size>>1)) << 16;
      vel[0].x = -vel[0].x;
      pair_vel.x = -pair_vel.x;
   }
  
   if ((pos[0].y>>16) + (size>>1) > bbound)
   {
      pos[0].y = (bbound - (size>>1)) << 16;
      vel[0].y = -vel[0].y;
      pair_vel.y = -pair_vel.y;
   }
   if ((pos[0].y>>16) - (size>>1) < 0)
   {
      pos[0].y = (size>>1) << 16;
      vel[0].y = -vel[0].y;
      pair_vel.y = -pair_vel.y;
   }

   //Clip second in pair
   if ((pos[1].x>>16) + (size>>1) > rbound)
   {
      pos[1].x = (rbound - (size>>1)) << 16;
      vel[1].x = -vel[1].x;
   }
   if ((pos[1].x>>16) - (size>>1) < lbound)
   {
      pos[1].x = (lbound + (size>>1)) << 16;
      vel[1].x = -vel[1].x;
   }
  
   if ((pos[1].y>>16) + (size>>1) > bbound)
   {
      pos[1].y = (bbound - (size>>1)) << 16;
      vel[1].y = -vel[1].y;
   }
   if ((pos[1].y>>16) - (size>>1) < 0)
   {
      pos[1].y = (size>>1) << 16;
      vel[1].y = -vel[1].y;
   }
   
   //Check to see if they are drifting too far apart
   int dist = ((pos[0].x-pos[1].x)>>16)*((pos[0].x-pos[1].x)>>16)+
              ((pos[0].y-pos[1].y)>>16)*((pos[0].y-pos[1].y)>>16);
              
   if (dist > (size*size) / 4)
   {
      int cur_len = ((vel[0].x>>8) * (vel[0].x>>8) +
                    (vel[0].y>>8) * (vel[0].y>>8));
      
      vel[0].x += ((pos[1].x-pos[0].x) >> ATTRACT) / sqrt(dist);
      vel[0].y += ((pos[1].y-pos[0].y) >> ATTRACT) / sqrt(dist);
      
      vel[1].x += ((pos[0].x-pos[1].x) >> ATTRACT) / sqrt(dist);
      vel[1].y += ((pos[0].y-pos[1].y) >> ATTRACT) / sqrt(dist);
   }
}

// Render the blob pair into the density buffer
void BlobPair::render (void)
{
   int tx, ty, range, top, bottom;
   int t_yoff, b_yoff, toff, boff;

   int off0 = ((pos[0].y>>16)-(size>>1)) * LavaLamp::WIDTH +
              ((pos[0].x>>16)-(size>>1));

   int off1 = ((pos[1].y>>16)-(size>>1)) * LavaLamp::WIDTH +
              ((pos[1].x>>16)-(size>>1));   

   //Clipping
   top = (pos[0].y >> 16) - (size>>1);
   bottom = (pos[0].y >> 16) + (size>>1);
   
   if (top >= LavaLamp::LAMP_BOTTOM) range = 0;
   else if (bottom > LavaLamp::LAMP_BOTTOM) range = LavaLamp::LAMP_BOTTOM+2 - top;
   else range = size;

   t_yoff = 0, b_yoff = 0;   
   for (ty=0; ty < range; ty++)
   {
      for (tx=0; tx < size; tx++)
      {
         toff = t_yoff + tx;
         boff = b_yoff + tx;
               
         if (layer[off0+boff] + blob_map[toff] > 255)
            layer[off0+boff] = 255;
         else
            layer[off0+boff] += blob_map[toff];
      }
            
      t_yoff += size;
      b_yoff += LavaLamp::WIDTH;
   }
   
   //Clipping
   top = (pos[1].y >> 16) - (size>>1);
   bottom = (pos[1].y >> 16) + (size>>1);
   
   if (top >= LavaLamp::LAMP_BOTTOM) range = 0;
   else if (bottom > LavaLamp::LAMP_BOTTOM) range = LavaLamp::LAMP_BOTTOM+2 - top;
   else range = size;

   t_yoff = 0, b_yoff = 0;
   for (ty=0; ty < range; ty++)
   {
      for (tx=0; tx < size; tx++)
      {
         toff = t_yoff + tx;
         boff = b_yoff + tx;
               
         if (layer[off1+boff] + blob_map[toff] > 255)
            layer[off1+boff] = 255;
         else
            layer[off1+boff] += blob_map[toff];
      }
            
      t_yoff += size;
      b_yoff += LavaLamp::WIDTH;
   }  
}
