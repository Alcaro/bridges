#include "arlib.h"

//Input:
//map - a string of the form " 2 \n271\n 2 \n"; width and height are calculated automatically
//error - 0 if unsolvable, 1 if there is a solution, 2 if there are two or more solutions
//maxdepth - maximum number of nested guesses that may be made; if exceeded, 'error' is -1
//neededdepth - actual number of guesses made, 0-2
//
//Output:
//A string in the same shape as the input, containing the number of bridges that exit at the right
// of the given cell. Ocean tiles remain spaces.
//To find the bridges from the left, check the previous tile. To find vertical bridges, start at an
// edge and fill in everything.
//
//Runtime: O(s^4d), where s is width*height and d is maxdepth. (Probably less, but I can't prove it.)
//Higher depth can solve more maps, but maps needing high depth are also hard to solve for humans.
//Memory use: O(s*d)
string map_solve(cstring map, int* error = NULL, int maxdepth = 2, int* neededdepth = NULL);

//May return fewer islands than requested if all tiles are taken by islands or bridges.
//The return value is guaranteed to have a solution, but may not necessarily have a unique one.
string map_generate(int width, int height, int islands);


class game : nocopy {
public:
	enum key_t { k_right, k_up, k_left, k_down, k_confirm, k_cancel };
	
	//Call when anything is pressed. Hold/release should be ignored.
	virtual void key(key_t k) = 0;
	//Call on click. Move, hold, drag and release should be ignored.
	virtual void click(int x, int y) = 0;
	
	//'pixels' should be a 640x480 array. Pixels will be 0RGB8888, native endian.
	virtual void run(uint32_t* pixels) = 0;
	
	virtual ~game() {}
	
	static game* create();
};
