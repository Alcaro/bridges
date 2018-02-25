#include "game.h"

class game_impl : public game {
public:
	int color = 255;
	
	void key(key_t k)
	{
		color = k*50;
	}
	
	void click(int x, int y)
	{
		color = x&255;
	}
	
	void run(uint32_t* pixels)
	{
		for (int i=0;i<640*480;i++) pixels[i] = color;
		
		for (int y=20;y<40;y++)
		for (int x=20;x<40;x++)
		{
			pixels[y*640+x] = 0x00FF00;
		}
		//color ^= 64;
	}
	
};

game* game::create() { return new game_impl(); }
