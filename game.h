#include "arlib.h"

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
		int8_t population; // -1 for ocean, -2 for reef; valid for all parts of this large island
		int8_t totbridges; // unspecified for large islands; all other members are valid for larges
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

//Returns whether the given map is solvable. Assumes input bridges are correct.
//If the input bridges are not correct, may falsely report the map unsolvable, or return an invalid solution.
//If solvable, the relevant bridges are added to the map. If not solvable, 'map' remains unchanged.
//If there are multiple solutions, nothing is guaranteed about which of them is returned.
bool solver_solve(gamemap& map);
//Give it a solved map as input. Returns another solution for the same map, if one exists.
//If that's the only solution, returns false and doesn't change the input.
//If the input is not solved, may yield any valid return value.
//If there are multiple solutions, nothing is guaranteed about which of them is returned, except the input is not returned.
bool solver_solve_another(gamemap& map);
//Returns an estimate. If the map is unsolvable, the return value is unspecified.
//If the map has multiple solutions, it gets whiny.
int solver_difficulty(const gamemap& map);
//Returns index (y*100+x) to an island where a bridge can be determined buildable.
//If skip is 1, the second available hint is returned. If no further bridges can be determined buildable on this map, returns -1.
//Can return -1 for skip=0 if the map is unsolvable or input bridges are incorrect.
int16_t solver_hint(const gamemap& map, int skip = 0);
