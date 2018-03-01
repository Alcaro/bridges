#define MARCH2018
#include "game.h"

class solver {
	int width;
	int height;
	int size;
	
	int not_ocean; // index to an arbitrary island
	int n_islands;
	
	array<int8_t> map; // number of bridges, 1-8, or -1 for ocean
	                   // (0 is legal too, but unless that's the only non-ocean, that map is impossible.)
	array<uint16_t> state; // the 1<<0 bit means 'it's possible that 0 bridges exit this tile to the right'
	                       // 1<<(1,2,3) means up, left and down, respectively
	                       // the 1<<(4..7) bits mean it's possible to have 1 bridge in that direction
	                       // 1<<(8..11) means 2 bridges
	                       // 1<<12 is used to check whether all islands are connected; outside that function, always false
	                       // 1<<(13..15) are completely unused
	                       // ocean tiles have this set too; for them, left and right are always equal, as are up/down
	
	array<int> towalk;
	array<uint8_t> towalk_bits;
	int n_towalk;
	
	//Sets state[ix] to newbits, and updates the corresponding bits for the connecting islands.
	//Can only clear bits, can't set.
	//Returns false if any fill_known_at does (i.e. if doing that makes the map impossible).
	bool set_state(int ix, uint16_t newstate)
	{
		uint16_t oldstate = state[ix];
		
		int offsets[4] = { 1, -width, -1, width };
		
		for (int dir=0;dir<4;dir++)
		{
			int offset = offsets[dir];
			int src_bits = ((oldstate^newstate) & (0x0111<<dir));
			int dst_bits = (dir&2) ? (src_bits>>2) : (src_bits<<2);
			if ((oldstate^newstate) & (0x0111<<dir))
			{
				int ix2 = ix;
				do
				{
					state[ix2] ^= src_bits;
					ix2 += offset;
					state[ix2] ^= dst_bits;
				} while (map[ix2 % size] == -1);
			}
		}
		
		int bank = ix / size;
		ix %= size;
		
		for (int dir=0;dir<4;dir++)
		{
			int offset = offsets[dir];
			if ((oldstate^newstate) & (0x0111<<dir))
			{
				//don't merge those calls, it's a tiny slowdown for whatever reason
				int ix2 = ix + offset;
				while (map[ix2] == -1)
				{
					if (!fill_known_at(bank, ix2)) return false;
					ix2 += offset;
				}
				if (!fill_known_at(bank, ix2)) return false;
			}
		}
		
		return true;
	}
	
	//Returns popcount of the middle nybble, ignoring the other bits.
	__attribute__((always_inline)) int popcount_mid(uint16_t in)
	{
		uint64_t pack = 0x4332322132212110ULL;
		return ((pack >> ((in>>2) & 0x3C)) + (pack >> ((in>>6) & 0x3C)))&15;
	}
	
	//Removes any possibilities from the given index that would create crossing bridges,
	// or require more or less bridges than that tile allows.
	//Returns false if that map is now known unsolvable. If so, the state is now undefined; if not, the state is valid.
	bool fill_known_at(int bank, int ix)
	{
		int required = map[ix];
		
		ix += bank*size;
		uint16_t flags = state[ix];
		
		if (required != -1)
		{
			//check how many bridges exit from each tile
			//keep only the lowest and highest bits
			// (the <<8s would be unneeded if '0 or 2 but not 1' was impossible, but I don't think it is)
			uint16_t flags_min = (flags & ~(flags<<4 | flags<<8));
			uint16_t flags_max = flags;
			
			int min = popcount_mid(flags_min | flags_min>>4);
			int max = popcount_mid(flags_max | flags_max>>4);
			
			if (min > required || max < required) return false;
			if (min == max) return true;
			
			//if no further bridges can be built, ban the options
			if (min == required) return set_state(ix, flags_min);
			//if all possible bridges must be built, do that
			if (max == required) return set_state(ix, (flags & ~(flags>>4 | flags>>8)));
			
			//if only one more bridge is free, lock all two-bridge possibilities
			if (min == max-1) return true; // no two-bridge possibilities open -> skip checks
			
			uint16_t flags_min_minus1 = (flags & ~(flags<<8));
			uint16_t flags_max_minus1 = (flags & ~(flags>>8));
			if (min == required-1 && flags_min_minus1 != flags) return set_state(ix, flags_min_minus1);
			if (max == required+1 && flags_max_minus1 != flags) return set_state(ix, flags_max_minus1);
		}
		else
		{
			//ocean tile with mandatory vertical bridge and allowed horizontal -> require zero horizontal
			if ((flags & 0x0550) != 0 && (flags & 0x000A) == 0)
			{
				return set_state(ix, flags & 0x0AAF);
			}
			//mandatory horizontal -> ban vertical
			if ((flags & 0x0AA0) != 0 && (flags & 0x0005) == 0)
			{
				return set_state(ix, flags & 0x055F);
			}
		}
		return true;
	}
	
	//Returns whether the given two tiles are disjoint, i.e. you cannot reach one from the other by following possible existing bridges.
	bool is_disjoint(int bank)
	{
//#error stop upon reaching a specific tile
//#error then start at both simultaneously (setting 0x1000/0x2000 depending on source), stop when reaching anything reachable from the other
//#error then keep count of how many in towalk from each one, running until one empties (unless they reach each other first)
//#error use a separate buffer, memset it (only one, rather than one per bank)
		int banksize = bank*size;
		
		memset(towalk_bits.ptr(), 0, sizeof(uint8_t)*towalk_bits.size());
		
		n_towalk = 1;
		towalk_bits[not_ocean] = 9;
		towalk[0] = not_ocean;
		
		int visited = 0;
		while (n_towalk)
		{
			int ix = towalk[--n_towalk];
			visited++;
			
			int flags = state[banksize + ix];
			int sourcedir = towalk_bits[ix]; // 1 - don't go right; 2 - don't go up; 3 - don't go left; 4 - don't go down; 9 - go anywhere
			
			flags &= ~(0x0110>>1 << sourcedir);
			
			//unrolled for performance
			
			//right
			if ((flags & 0x0110))
			{
				int ix2 = ix;
				do {
					ix2 += 1;
				} while (map[ix2] == -1);
				
				if (towalk_bits[ix2] == 0)
				{
					towalk_bits[ix2] = 3;
					towalk[n_towalk++] = ix2;
				}
			}
			
			//up
			if ((flags & 0x0220))
			{
				int ix2 = ix;
				do {
					ix2 += -width;
				} while (map[ix2] == -1);
				
				if (towalk_bits[ix2] == 0)
				{
					towalk_bits[ix2] = 4;
					towalk[n_towalk++] = ix2;
				}
			}
			
			//left
			if ((flags & 0x0440))
			{
				int ix2 = ix;
				do {
					ix2 += -1;
				} while (map[ix2] == -1);
				
				if (towalk_bits[ix2] == 0)
				{
					towalk_bits[ix2] = 1;
					towalk[n_towalk++] = ix2;
				}
			}
			
			//down
			if ((flags & 0x0880))
			{
				int ix2 = ix;
				do {
					ix2 += width;
				} while (map[ix2] == -1);
				
				if (towalk_bits[ix2] == 0)
				{
					towalk_bits[ix2] = 2;
					towalk[n_towalk++] = ix2;
				}
			}
		}
		
		if (visited != n_islands) return true;
		
		return false;
	}
	
	bool finished(int bank)
	{
		for (int y=0;y<height;y++)
		for (int x=0;x<width;x++)
		{
			uint16_t flags = state[bank*size + y*width + x];
			if (flags & (flags>>4 | flags>>8))
			{
				return false;
			}
		}
		return true;
	}
	
	void copy_bank(int dst, int src)
	{
		memcpy(state.slice(dst*size, size).ptr(),
		       state.slice(src*size, size).ptr(),
		       sizeof(uint16_t)*size);
	}
	
	int solve_rec(int depthat, int maxdepth)
	{
		if (finished(depthat)) return 1;
		
		if (depthat == maxdepth) return -1;
		
	fill_again:
		bool didsomething = false;
		
		for (int y=0;y<height;y++)
		for (int x=0;x<width;x++)
		{
			if (map[y*width + x] == -1) continue;
			
			int ix      =  depthat   *size + y*width + x;
			int ix_next = (depthat+1)*size + y*width + x;
			uint16_t flags = state[ix];
			
			//right and up (ignore left/down, they'd only find duplicates)
			for (int dir=0;dir<=1;dir++)
			{
				//if multiple bridge numbers are allowed in this direction
				if (flags & (flags>>4 | flags>>8) & (0x0111<<dir))
				{
					int set_mask = (dir ? 0x0DDD : 0x0EEE);
					int n_solvable = 0;
					for (int n=0;n<=2;n++)
					{
						int dirbit = (0x0001<<(n*4)<<dir);
						if (flags & dirbit)
						{
							copy_bank(depthat+1, depthat);
							int ret = 1;
							//assuming a bridge exists can ban a crossing bridge and make it disjoint
							//assuming a bridge exists can require others to exist or not exist, making it disjoint
							//for example (* being ocean):
							// 1*2
							//1  3
							//1  3
							// 2 3
							//assume there is no bridge at the *. then it must be vertical, which isolates the 1s at the left.
							if (ret == 1 && !set_state(ix_next, flags & (set_mask | dirbit))) ret = 0;
							if (ret == 1 && is_disjoint(depthat+1)) ret = 0;
							if (ret == 1) ret = solve_rec(depthat+1, maxdepth);
							
							if (ret == -1) n_solvable = -99;
							if (ret == 0)
							{
								if (!set_state(ix, state[ix] & ~dirbit)) return 0;
								didsomething = true;
								
								if (is_disjoint(depthat)) return 0;
								if (finished(depthat)) return 1;
							}
							if (ret == 1)
							{
								n_solvable++;
								if (n_solvable > 1) return 2;
							}
							if (ret == 2) return 2;
						}
					}
					
					if (!(state[ix] & (0x0111<<dir))) return 0; // if all options are impossible, drop it
				}
			}
		}
		
		if (didsomething) goto fill_again;
		
		return -1;
	}
	
public:
	
	//Takes a string of the form " 2 \n271\n 2 \n". Calculates width and height automatically.
	//Must be rectangular; trailing spaces must exist on all lines, and a trailing linebreak must exist. Non-square inputs are fine.
	void init(cstring inmap)
	{
		width = 0;
		while (inmap[width] != '\n') width++;
		height = 0;
		while (inmap[(width+1) * height] != '\0') height++;
		size = height * width;
		
		n_islands = 0;
		
		map.resize(size);
		state.resize(size);
		towalk.resize(size);
		towalk_bits.resize(size);
		
		for (int y=0;y<height;y++)
		for (int x=0;x<width;x++)
		{
			char mapchar = inmap[y*(width+1) + x];
			int8_t neighbors;
			if (mapchar == ' ') neighbors = -1;
			else neighbors = mapchar-'0';
			map[y*width + x] = neighbors;
			
			state[y*width + x] = 0x000F; // all tiles can have 0 bridges towards anywhere, including oceans and things pointing outside the map
			if (neighbors != -1)
			{
				n_islands++;
				
				not_ocean = y*width + x;
				for (int x2=x-1;x2>=0;x2--)
				{
					if (map[y*width + x2] != -1)
					{
						state[y*width + x2] |= 0x0111; // right
						state[y*width + x ] |= 0x0444; // left
						x2++;
						while (x2 != x)
						{
							state[y*width + x2] |= 0x0555;
							x2++;
						}
						break;
					}
				}
				for (int y2=y-1;y2>=0;y2--)
				{
					if (map[y2*width + x] != -1)
					{
						state[y2*width + x] |= 0x0888; // down
						state[y *width + x] |= 0x0222; // up
						y2++;
						while (y2 != y)
						{
							state[y2*width + x] |= 0x0AAA;
							y2++;
						}
						break;
					}
				}
			}
		}
	}
	
	//maxdepth is number of nested guesses that may be made in an attempt to solve the map.
	//Runtime is O(s^(4*(d+1))), where s is width*height and d is max depth. (Probably less, but I can't prove it.)
	//Higher depth can solve more maps, but maps needing high depth are also hard to solve for humans.
	//Returns:
	//-1 if it's unsolvable with that max depth, but will give another answer on higher depth
	//0 if unsolvable
	//1 if there is a unique solution
	//2 if there are multiple valid solutions
	//If 1, you may call solution().
	int solve(int maxdepth, int* neededdepth)
	{
		state.reserve(size*(maxdepth+1));
		
		for (int ix=0;ix<size;ix++)
		{
			if (!fill_known_at(0, ix)) return 0;
		}
		if (is_disjoint(0)) return 0;
		
		for (int depth = 0; depth <= maxdepth; depth++)
		{
			if (neededdepth) *neededdepth = depth;
			int ret = solve_rec(0, depth);
			if (ret >= 0) return ret;
		}
		return -1;
	}
	
	//The solution is in the same shape as the input, and contains the number of bridges that exit at the right of the given cell.
	//Ocean tiles remain spaces.
	//To find the bridges from the left, check the previous tile. To find vertical bridges, start at an edge and fill in everything.
	string solution()
	{
		string ret = "";
		for (int y=0;y<height;y++)
		{
			for (int x=0;x<width;x++)
			{
				if (map[y*width + x] == -1)
				{
					ret += ' ';
					continue;
				}
				uint16_t flags = state[y*width + x];
				if (flags & (flags>>4 | flags>>8)) return ""; // unsolved
				if (flags & (0x0100)) ret += '2';
				if (flags & (0x0010)) ret += '1';
				if (flags & (0x0001)) ret += '0';
			}
			ret += '\n';
		}
		return ret;
	}
	
	void print(int bank)
	{
		printf("%i,%i d=%i\n", width, height, bank);
		
		for (int y=0;y<height;y++)
		{
			for (int x=0;x<width;x++)
			{
				printf("%.4X ", state[bank*size + y*width + x]);
			}
			puts("");
		}
		puts(finished(bank) ? "finished" : "unfinished");
	}
	
	solver(cstring map, int* perror, int maxdepth, int* neededdepth, string* psolution)
	{
		init(map);
		int error = solve(maxdepth, neededdepth);
		if (perror) *perror = error;
		*psolution = solution();
	}
};

string map_solve(cstring map, int* error, int maxdepth, int* neededdepth)
{
	string ret;
	solver s(map, error, maxdepth, neededdepth, &ret);
	return ret;
}


static void test_split(cstring in, string& map, string& solution)
{
	array<cstring> lines = in.csplit("\n");
	
	for (size_t i=0;i<lines.size()-1;i+=2)
	{
		map      += lines[i  ]+"\n";
		solution += lines[i+1]+"\n";
	}
}

static void test_one(int maxdepth, cstring test)
{
	string map;
	string solution_good;
	test_split(test, map, solution_good);
	
	int result;
	int depth;
	string solution = map_solve(map, &result, maxdepth, &depth);
	assert_eq(result, 1);
	assert_eq(solution, solution_good);
	assert_eq(depth, maxdepth);
}

static void test_error(int maxdepth, int exp, cstring map)
{
	int result;
	int depth;
	map_solve(map, &result, maxdepth, &depth);
	assert_eq(result, exp);
	assert_eq(depth, maxdepth);
}


test("solver", "", "solver")
{
	testcall(test_one(0, // just something simple
		" 2 \n" /* */ " 0 \n"
		"271\n" /* */ "210\n"
		" 2 \n" /* */ " 0 \n"
	));
	testcall(test_one(0, // requires multiple passes
		" 1  3\n" /* */ " 1  0\n"
		"2 1  \n" /* */ "1 0  \n"
		"2  23\n" /* */ "1  10\n"
	));
	testcall(test_one(0, // bridges may not cross
		"464\n" /* */ "220\n"
		"4 4\n" /* */ "0 0\n"
		"2 2\n" /* */ "0 0\n"
		" 2 \n" /* */ " 0 \n"
	));
	testcall(test_one(1, // requires guessing
		"2 2\n" /* */ "1 0\n"
		"222\n" /* */ "110\n"
	));
	testcall(test_one(1, // all islands must be connected
		"22\n" /* */ "10\n"
		"22\n" /* */ "10\n"
	));
	testcall(test_one(0, // not sure if this enters the fill-mandatory-bridges function, but it has caught a few bugs
		"1 2  \n" /* */ "1 0  \n"
		" 1 1 \n" /* */ " 0 0 \n"
		"14441\n" /* */ "11110\n"
		" 1 1 \n" /* */ " 0 0 \n"
		"  2 1\n" /* */ "  1 0\n"
	));
	testcall(test_one(0, // make islands trickier
		"4    4 \n" /* */ "2    0 \n"
		"  4 4  \n" /* */ "  2 0  \n"
		"    682\n" /* */ "    220\n"
		" 282   \n" /* */ " 220   \n"
		"  4 4  \n" /* */ "  2 0  \n"
		"4    4 \n" /* */ "2    0 \n"
	));
	testcall(test_one(1, // also requires guessing, multiple times
		"2  2\n" /* */ "1  0\n"
		" 22 \n" /* */ " 10 \n"
		"  33\n" /* */ "  10\n"
		" 22 \n" /* */ " 10 \n"
		"2  2\n" /* */ "1  0\n"
	));
	testcall(test_one(1, // nasty amount of cycles because must annoy island tracker
		"2          2\n" /* */ "1          0\n"
		" 2        2 \n" /* */ " 1        0 \n"
		"  2  3   2  \n" /* */ "  1  1   0  \n"
		"   2 3  2   \n" /* */ "   1 1  0   \n"
		"    2  2    \n" /* */ "    1  0    \n"
		"       33   \n" /* */ "       10   \n"
		" 33         \n" /* */ " 10         \n"
		"    2  2    \n" /* */ "    1  0    \n"
		"   2    2   \n" /* */ "   1    0   \n"
		"  2      2  \n" /* */ "  1      0  \n"
		" 2    3   2 \n" /* */ " 1    1   0 \n"
		"2     3    2\n" /* */ "1     1    0\n"
	));
	// these require nested guesses (created by the game generator)
	testcall(test_one(2,
		"2  1\n" /* */ "0  0\n"
		"3233\n" /* */ "1110\n"
		" 131\n" /* */ " 100\n"
		"1 2 \n" /* */ "1 0 \n"
	));
	testcall(test_one(2,
		"1232\n" /* */ "0120\n"
		"2 11\n" /* */ "0 00\n"
		"5543\n" /* */ "2120\n"
		"23 2\n" /* */ "02 0\n"
	));
	testcall(test_one(2,
		"1332\n" /* */ "1100\n"
		"13 3\n" /* */ "10 0\n"
		"21  \n" /* */ "00  \n"
		"3 42\n" /* */ "1 10\n"
	));
	testcall(test_one(2,
		"3431\n" /* */ "2110\n"
		" 112\n" /* */ " 000\n"
		"32 3\n" /* */ "11 0\n"
		"2  1\n" /* */ "1  0\n"
	));
	
	testcall(test_error(1, 2, // has multiple solutions
		"33\n"
		"22\n"
	));
	testcall(test_error(2, 2, // this too
		" 31\n"
		"272\n"
		"131\n"
	));
	testcall(test_error(3, 2, // requires high depth
		"232\n"
		"343\n"
		"232\n"
	));
	testcall(test_error(3, 2, // takes forever
		"2332\n"
		"3443\n"
		"2332\n"
	));
}
