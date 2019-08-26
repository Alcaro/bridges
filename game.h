#include "arlib.h"

extern const char * const game_maps[];

class game : nocopy {
public:
	
	enum key_t { k_right, k_up, k_left, k_down, k_confirm, k_cancel };
	
	struct input {
		int mousex; // if the mouse is not in the game window, use x=-1 y=-1 click=false
		int mousey;
		bool lmousebutton;
		bool rmousebutton;
		
		uint8_t keys; // format: 1<<k_up | 1<<k_confirm
	};
	
	//Callers should treat savedat as an opaque binary blob.
	//The only permitted operations are passing pointers to memcpy/fwrite/etc, and passing them to a game object.
	struct savedat {
		uint8_t unlocked;
		bool seen_random_tutorial;
		
		litend<uint64_t> gen_seeds[3];
	};
	
	//'out' should be a 640x480 image. 0rgb8888 or xrgb8888 is recommended, xrgb is slightly faster.
	//All 640*480 pixels will be overwritten. They will be written non-sequentially, multiple times, and may be read.
	//Return value:
	// 1 - the frame was rendered
	// 0 - the frame wasn't rendered, the previous one should be shown for another 16ms.
	// -1 - the frame wasn't rendered, and there are no ongoing animations; the previous frame should be shown until the next user input
	//For the latter two, 'out' remains unchanged; caller is allowed to put the previous frame there and rerender that.
	//If can_skip is false, return value is always 1.
	virtual int run(const input& in, image& out, bool can_skip = false) = 0;
	
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
		int8_t population; // -1 for ocean, -2 for reef, 80-83 for castles; valid for all parts of a large island
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
	// The root must be joined to the tile below or right; it may not be on the bottom-right edge.
	// An island may not be able to connect to itself. Stick a reef in the hole if necessary.
	// For performance reasons, large islands should use reasonably short paths to the root. Don't create too silly patterns.
	//For islands with population 10 to 35, use A (10) to Z (35). Population above 35 is not allowed, it'd be too annoying to play.
	//Castles are 'rbyg' (lowercase), red, blue, yellow and green, respectively.
	// Castles must be joined with the tile below, right, AND below-right.
	//  This applies even if you don't intend to render the map; the solver requires that too.
	//Placing zero bridges must not be a solution.
	//
	//(A few tests break these rules. No other caller should.)
	void init(const char * inmap);
	
	void toggle(int x, int y, int dir);
	void reset();
	bool finished();
	
	//Returns whether the given map is solvable. Assumes all input bridges are correct.
	//If the input bridges are not correct, may falsely report the map unsolvable, or return an invalid solution.
	//If solvable, the solution is added to the map. If not solvable, 'map' remains unchanged.
	//If there are multiple solutions, one of them is returned. There is no guarantee about which one.
	bool solve();
	//Give it a solved map as input. Returns another solution for the same map, if one exists.
	//If that's the only solution, returns false and doesn't change the input.
	//If the input is not solved, undefined behavior.
	//If there are three or more solutions, nothing is guaranteed about which of them is returned, except the input is not returned.
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
		uint8_t width; // Size must be at least 1x1, though larger is needed to create actually useful maps
		uint8_t height;
		
		float density; // Use a value between 0.0 and 1.0; it will attempt to fill that fraction of the map with islands.
		               // Since some tiles will be taken by bridges, anything above (roughly) 0.4 will act identical.
		               // Reducing it further will considerably reduce difficulty.
		bool allow_dense; // Don't try to avoid maps where two islands are exactly beside each other.
		
		bool use_reef; // Include reefs in the returned map.
		bool use_large; // Include large islands in the returned map.
		bool use_castle; // Include castles in the returned map. If use_large is false, castles are the only large islands.
		                 // If set, size must be at least 3x3.
		
		bool allow_multi; // Allow returning a map with multiple valid solutions. Not recommended.
		float difficulty; // Use a value between 0.0 and 1.0. Higher is harder.
		                  // The lowest possible values are mostly silly maps with only two islands,
		                  // because all possible slots nearby are taken by reefs.
		unsigned quality; // Higher quality takes longer to generate, but better matches
		                  // the requested parameters. 1000 to 5000 is recommended.
		function<bool(unsigned iter)> progress; // Will be called with 'iter' increasing until it reaches 'quality'.
		                                        // Not guaranteed to increase monotonically.
		                                        // Can be called with iter > quality if the requested parameters
		                                        //   make it hard to find non-ambiguous maps.
		                                        // Return false to stop before iter reaches quality.
		                                        // To create arbitrarily many maps, using only this to stop it,
		                                        //   set quality to UINT_MAX.
	};
	
	class generator {
		genparams par; // Read only after construction, safe to read from any thread.
		
		mutex mut;
		semaphore sem;
		
		//None of these may be accessed without holding the mutex.
		uint64_t randseed;
		unsigned n_started; // initialized to number of threads
		unsigned n_valid = 0;
		unsigned n_finished = 0;

		unsigned diff_min = 999999;
		unsigned diff_max = 0;
		uint64_t best_seed;
		unsigned best_diff = 999999;
		
		bool stop = false;
		uint8_t n_more_threads;
		
		struct workitem {
			uint64_t seed;
			bool valid;
			unsigned diff;
		};
		
		bool get_work(bool first, workitem& w);
		void do_work(workitem& w, gamemap& map);
		void finish_work(workitem& w);
		void threadproc();
		
	public:
		generator(const gamemap::genparams& par);
		void cancel(); // Safe to call multiple times.
		bool done(unsigned* progress);
		
		bool finish(gamemap& map); // If cancelled, fails and does nothing, or yields a map not matching genparams' quality.
		
		//Pass this value to unpack(). Passing any other value is likely to give results inconsistent with the generation parameters.
		//If the return value is 0, generation was cancelled.
		uint64_t pack();
		static void unpack(const gamemap::genparams& par, uint64_t seed, gamemap& map);
		~generator() { cancel(); }
	};
	
	bool generate(const genparams& par)
	{
		generator gen(par);
		return gen.finish(*this);
	}
	
	string serialize(); // Passing this return value to init() will recreate the map (minus placed bridges).
};
