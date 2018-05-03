#include "arlib.h"

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
	
	int numislands; // includes non-root island tiles
	
	struct island {
		int8_t population; // -1 for ocean, -2 for reef, 80-83 for castles; valid for all parts of this large island
		int8_t totbridges; // unspecified for large islands; all other members are valid for larges
		int16_t rootnode; // -1 for ocean, y*100+x for most islands, something else for large island non-roots
		
		//1 if the next tile in that direction is an island, 2 if there's one ocean tile before the next island
		//-1 if bridges there are illegal (would run into the edge of the map, or a reef)
		//valid for both islands and ocean tiles
		//[0] is right, [1] is up, [2] left, [3] down
		int8_t bridgelen[4];
		
		//number of bridges exiting this tile in the given direction, 0-2
		//set for ocean tiles too; for them, [0]=[2] and [1]=[3]
		//if that island is part of the current one (rootnode equal and bridgelen 1), 3 bridges
		uint8_t bridges[4];
	};
	
	island map[100][100];
	
	bool has_castles;
	int16_t an_island[5]; // [0] is any island root, [1] is any red castle root, [2] blue, etc; -1 if no such thing
	
	//these two are used only inside finished(), to check if the map is connected
	bool visited[100*100];
	int16_t towalk[100*100];
	
	island& get(uint16_t key) { return map[key/100][key%100]; }
	
	//Maps are strings, one character per tile. Each row is terminated by a \n; the entire map ends with a NUL after a \n.
	//The map must be rectangular, and 100 tiles or less in both directions.
	//Space is ocean. Digits are islands.
	//For example: "  2  \n2 7 1\n  2  \n\0".
	//# is a reef; can't build anything across that.
	//Large islands are > ^ < v, ultimately pointing to a character defining the population (the 'root').
	// There must be a root, and the root must be an island (not ocean or reef); loops are not allowed.
	// If 10 bridges is possible, the root must be joined to the tile below or right; it may not be on the bottom-right edge.
	//  For simplicity, it's better to always avoid large islands with roots at the bottom-right.
	// An island may not be able to connect to itself. Stick in a reef if necessary.
	// For performance reasons, large islands should use reasonably short paths to the root. Don't create too silly patterns.
	//For islands with population 10 to 35, use A (10) to Z (35). Population above 35 is not allowed, it'd be too annoying to play.
	//Castles are 'rbyg' (lowercase), red, blue, yellow and green, respectively.
	// Castles must be joined with the tile below, right, AND below-right.
	//  This applies even if you don't intend to render the map; the solver requires that too.
	//For human playable maps, placing zero bridges must not be a solution.
	void init(const char * inmap);
	
	void toggle(int x, int y, int dir);
	void reset();
	bool finished();
	
	//Returns whether the given map is solvable. Assumes all input bridges are correct.
	//If the input bridges are not correct, may falsely report the map unsolvable, or return an invalid solution.
	//If solvable, the solution is added to the map. If not solvable, 'map' remains unchanged.
	//If there are multiple solutions, nothing is guaranteed about which of them is returned.
	bool solve();
	//Give it a solved map as input. Returns another solution for the same map, if one exists.
	//If that's the only solution, returns false and doesn't change the input.
	//If the input is not solved, may yield any valid return value.
	//If there are multiple solutions, nothing is guaranteed about which of them is returned, except the input is not returned.
	bool solve_another();
	//The return value is approximate, and humans may sort difficulty differently from this function.
	//(Though humans aren't consistent with each other either.)
	//If the map has zero, or more than one, solution, undefined behavior.
	//Bridges in the input are ignored.
	uint32_t difficulty();
	//Acts like calling solve() and returning difficulty(). Returns 0 if unsolvable.
	//Due to not having to track the difficulty, solve() is faster.
	uint32_t solve_difficulty();
	//Given a half-finished map, returns a hint on which islands the next bridge should be built from.
	//Hints are returned by setting the 'visited' flags on the relevant islands.
	//If the map is unsolvable or contains incorrect bridges, may return zero hints, or recommend additional incorrect bridges.
	//If there are multiple solutions, the hints point towards an arbitrary one.
	void hint();
	
	struct genparams {
		uint8_t width; // Size must be at least 1x1.
		uint8_t height;
		
		float density; // Use a value between 0.0 and 1.0; it will attempt to fill that fraction of the map with islands.
		               // Since some tiles will be taken by bridges, the highest possible values will act badly. 0.2 to 0.5 is recommended.
		bool allow_dense; // Allow returning a map where two islands are exactly beside each other.
		                  // Disable it if you have trouble seeing so short bridges.
		uint8_t max_brilen; // Maximum allowed bridge length. If zero, the generator will automatically pick one based on size and density.
		
		bool use_reef; // Include reefs in the returned map.
		bool use_large; // Include large islands in the returned map.
		bool use_castle; // Include castles in the returned map. If use_large is false, castles are the only large islands.
		                 // If set, size must be at least 3x3.
		
		bool allow_multi; // Allow returning a map with multiple valid solutions. Not recommended.
		float difficulty; // Use a value between 0.0 and 1.0. Higher is harder.
		unsigned quality; // Higher quality takes longer to generate, but better matches the requested parameters. 1000 to 5000 is recommended.
		function<bool(int iter)> quality_stop; // If quality is 0, this function will be repeatedly called instead. Return true to stop.
	};
	void generate(const genparams& par); // Replaces the map. The returned map will be solved.
	
	string serialize(); // Passing this return value to init() will recreate the map (minus placed bridges).
};
