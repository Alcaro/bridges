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
string map_solve(cstring map, int* error = NULL, int maxdepth = 2, int* neededdepth = NULL);

//May return fewer islands than requested if all tiles are taken by islands or bridges.
//The return value is guaranteed to have a solution, but the solution is not guaranteed to be unique.
string map_generate(int width, int height, int islands);


extern const char * const game_maps[];

class game : nocopy {
public:
	
	enum key_t { k_right, k_up, k_left, k_down, k_confirm, k_cancel };
	
	struct input {
		int mousex; // if the mouse is not in the game window, use x=-1 y=-1 click=false
		int mousey;
		bool mouseclick;
		
		uint8_t keys; // format: 1<<k_up | 1<<k_confirm
	};
	
	struct savedat {
		uint8_t unlocked;
	};
	
	//'out' should be a 640x480 image. 0rgb8888 or xrgb8888 is recommended, xrgb is slightly faster.
	//All 640*480 pixels will be overwritten. They will be written non-sequentially, multiple times, and may be read.
	virtual void run(const input& in, image& out) = 0;
	
	//Call this before exiting the game, and save the result to disk. Next session, pass that struct to create().
	//For the first run, call create() without a savedat.
	virtual void save(savedat& dat) = 0;
	
	virtual ~game() {}
	
	static game* create();
	static game* create(const savedat& dat);
};


class gamemap {
public:
	//known to be 100 or less
	//(UI supports 13x10, anything more goes out of bounds)
	int width;
	int height;
	
	int numislands; // includes non-root islands
	
	struct island {
		int8_t population; // -1 for ocean, -2 for reef
		int8_t totbridges;
		int16_t rootnode; // -1 for ocean, y*100+x for most islands, something else for large island non-roots
		
		//1 if the next tile in that direction is an island, 2 if there's one ocean tile before the next island
		//-1 if bridges there are illegal (would run into the edge of the map, or a reef)
		//valid for both islands and ocean tiles
		//[0] is right, [1] is up, [2] left, [3] down
		int8_t bridgelen[4];
		
		//number of bridges exiting this tile in the given direction, 0-2
		//set for ocean tiles too; for them, [0]=[2] and [1]=[3]
		//if that island is part of the current one (rootnode equal), 3 bridges
		uint8_t bridges[4];
	};
	
	island map[100][100];
	
	bool has_castles;
	int16_t an_island[5]; // [0] is any island root, [1] is any red castle root, [2] blue, etc; -1 if no such thing
	
	//these two are used only inside finished(), to check if the map is connected
	bool visited[100*100];
	int16_t towalk[100*100];
	
	island& get(uint16_t key) { return map[key/100][key%100]; }
	
	void init(const char * inmap);
	void toggle(int x, int y, int dir);
	void reset();
	bool finished();
};

int16_t solver_hint(const gamemap& map, int skip = 0);
bool solver_solve(gamemap& map);
bool solver_solve_another(gamemap& map);
int solver_difficulty(const gamemap& map);
