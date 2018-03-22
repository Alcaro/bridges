#include "arlib.h"

//Input:
//map - a string of the form " 2 \n271\n 2 \n"; width and height are calculated automatically (both must be 100 or smaller)
//error - 0 if unsolvable, 1 if there is a solution, 2 if there are two or more solutions
//maxdepth - maximum number of nested guesses that may be made; if exceeded, 'error' is -1
//neededdepth - actual number of guesses made
//
//Output:
//A string in the same shape as the input, containing the number of bridges that exit at the right
// of the given cell. Linebreaks and ocean tiles remain unchanged.
//To find the bridges from the left, check the previous tile. To find vertical bridges, start at an
// edge and fill in everything.
//
//Runtime: O(s^4d), where s is width*height and d is maxdepth. (Probably less, but I can't prove it.)
//Higher depth can solve more maps, but maps needing high depth are also hard to solve for humans.
//Memory use: O(s*d)

//TODO: create more tile types:
//- islands accepting an arbitrary number of bridges
//- castles; every island must be connected to a castle, but they may not be connected to each other
//- reef; can't build bridges across that
string map_solve(cstring map, int* error = NULL, int maxdepth = 2, int* neededdepth = NULL);

//May return fewer islands than requested if all tiles are taken by islands or bridges.
//The return value is guaranteed to have a solution, but the solution is not guaranteed to be unique.
string map_generate(int width, int height, int islands);


extern const char * const game_maps[30];
class game : nocopy {
public:
	
	enum key_t { k_right, k_up, k_left, k_down, k_confirm, k_cancel };
	
	struct input {
		int mousex; // if the mouse is not in the game window, use x=-1 y=-1 click=false
		int mousey;
		bool mouseclick;
		
		uint8_t keys; // format: 1<<k_up | 1<<k_confirm
	};
	
	//'pixels' should be a 640x480 array. Pixels will be 0RGB8888, native endian.
	//It's allowed to stick random stuff between the rows. If so, increase the stride.
	//All 640*480 pixels will be overwritten. They will be written non-sequentially, and may be read.
	virtual void run(const input& in, uint32_t* pixels, size_t stride = sizeof(uint32_t)*640) = 0;
	
	virtual ~game() {}
	
	static game* create();
};
