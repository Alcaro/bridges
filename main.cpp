#include "arlib.h"

//int i=0;
class solver {
	int width;
	int height;
	int size;
	
	int not_ocean; // index to an arbitrary island
	
	array<int8_t> map; // number of bridges, 1-8, or -1 for ocean
	                   // (0 is legal too, but unless that's the only non-ocean, that map is impossible.)
	array<uint16_t> state; // the 1<<0 bit means 'it's possible that 0 bridges exit this tile to the right'
	                       // 1<<(1,2,3) means up, left and down, respectively
	                       // the 1<<(4,5,6,7) bits mean it's possible to have 1 bridge in that direction
	                       // 1<<(8..11) means 2 bridges
	                       // 1<<(12..15) are unused, always 0
	                       // (TODO: may be usable for temp stuff, or a 'is ocean' flag)
	                       // ocean tiles have this set too; for them, left and right are always equal, as are up/down
	
	//Sets state[ix] to newbits, and updates the corresponding bits for the connecting islands.
	//Can only clear bits, can't set.
	//If call_fill_known is set, calls fill_known for all changed tiles.
	//Returns false if any call_fill_known does.
	bool set_state(int ix, uint16_t newstate, bool call_fill_known_at = false)
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
		
		if (call_fill_known_at)
		{
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
		}
		
		return true;
	}
	
	//Returns only popcount of the middle nybble.
	__attribute__((always_inline)) int popcount_mid(uint16_t in)
	{
		in -= ((in>>1) & 0x0550);
		in = (in&0x0330) + ((in>>2) & 0x0330);
		return ((in + (in>>4)) & 0x0F0F0F0F) * 0x01010101 >> 24;
	}
	
	//Returns false if map is now known impossible.
	bool fill_known_at(int bank, int ix)
	{
		int required = map[ix];
		
		ix += bank*size;
		uint16_t flags = state[ix];
		
		if (required != -1)
		{
			//check how many bridges exit from each tile
			//keep only the lowest and highest bits (the <<8s would be unneeded if '0 or 2 but not 1' was impossible, but I don't think it is)
			uint16_t flags_min = (flags & ~(flags<<4 | flags<<8));
			uint16_t flags_max = flags;
			
			int min = popcount_mid(flags_min | flags_min>>4);
			int max = popcount_mid(flags_max | flags_max>>4);
			
			if (min > required || max < required) return false;
			if (min == max) return true; // no options left
			
			//if no further bridges can be built, do that
			if (min == required) return set_state(ix, flags_min, true);
			//if all possible bridges must be built, do that
			if (max == required) return set_state(ix, (flags & ~(flags>>4 | flags>>8)), true);
			
			//if only one more bridge is free, lock all two-bridge possibilities
			//(with a continue above, to avoid putting some bridges back)
			if (min == max-1) return true; // no two-bridge possibilities open
			
			uint16_t flags_min_minus1 = (flags & ~(flags<<8));
			uint16_t flags_max_minus1 = (flags & ~(flags>>8));
			if (min == required-1 && flags_min_minus1 != flags) return set_state(ix, flags_min_minus1, true);
			if (max == required+1 && flags_max_minus1 != flags) return set_state(ix, flags_max_minus1, true);
		}
		else
		{
			//ocean tile with mandatory vertical bridge and allowed horizontal -> require zero horizontal
			if ((flags & 0x0550) != 0 && (flags & 0x000A) == 0)
			{
				return set_state(ix, flags & 0x0AAF, true);
			}
			//mandatory horizontal -> ban vertical
			if ((flags & 0x0AA0) != 0 && (flags & 0x0005) == 0)
			{
				return set_state(ix, flags & 0x055F, true);
			}
		}
		return true;
	}
	
	//Removes any possibilities that would create crossing bridges, or require more or less bridges than that tile allows.
	//Returns false if that map is now known unsolvable. If so, the state is now undefined; if not, the state is valid.
	bool fill_known(int bank)
	{
		int ix=size;
		do
		{
			ix--;
			if (!fill_known_at(bank, ix)) return false;
		} while (ix);
		
		if (is_disjoint(bank)) return false;
		return true;
	}
	
	//Returns whether the map is disjoint, i.e. some bridges aren't connected to each other.
	bool is_disjoint(int bank)
	{
		int ix = bank*size + not_ocean;
		
		//I have four bits of unused state per tile, one per direction. All start out clear.
		//I will start at an arbitrary tile, facing an arbitrary direction.
		//If that path is blocked (no bridges can be built there), I'll turn right until it's not blocked.
		//I will then mark the path I'm facing as blocked, then follow it.
		//Once I arrive at the target, I will check if I've been here before (if any path is marked blocked).
		//If yes, I'll return to where I came from. If no, I'll turn left.
		//In both cases, I'll then repeat from the 'if that path is blocked' clause.
		//Once all four paths are taken, I will stop moving, and instead check all tiles on the map.
		//If any tile has an existing but untaken path, I will report the map unsolvable. Otherwise, I will clear all such flags.
		
		int dir = 0;
		int offsets[4] = { 1, -width, -1, width };
		//while there is an untaken but possible path from here
		while (true)
		{
			int targets = (~state[ix])&0xF000 & (state[ix]<<4 | state[ix]<<8);
			if (!targets) break;
			//rotate until path is walkable
			while (!(targets & (0x1000<<dir))) dir = (dir+1) & 3;
			
			//mark blocked
			state[ix] |= 0x1000<<dir;
			
			ix -= bank*size;
			//follow
			do {
				ix += offsets[dir];
			} while (map[ix] == -1);
			ix += bank*size;
			
			//if I've been here already, and it's possible to turn around, do that
			if ((state[ix]&0xF000) && !(state[ix]&(0x1000<<(dir^2)))) dir = dir^2;
			//otherwise, turn left
			else dir = (dir+3) & 3;
		}
		
		ix=size;
		do
		{
			ix--;
			int ix2 = bank*size + ix;
			//untaken but possible path = not connected = unsolvable
			if (map[ix]!=-1 && ((~state[ix2])&0xF000 & (state[ix2]<<4 | state[ix2]<<8)))
			{
				return true;
			}
			state[ix2] &= 0x0FFF;
		} while (ix);
		
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
	fill_again:
		if (!fill_known(depthat)) return 0;
		if (finished(depthat)) return 1;
		
		if (depthat == maxdepth) return -1;
		
		bool didsomething = false;
		
		for (int y=0;y<height;y++)
		for (int x=0;x<width;x++)
		{
			if (map[y*width + x] == -1) continue;
			
			int ix      =  depthat   *size + y*width + x;
			int ix_next = (depthat+1)*size + y*width + x;
			uint16_t flags = state[ix];
			
			//right and up (ignore left/down, they'd only find duplicates)
			for (int dir=0;dir<2;dir++)
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
							set_state(ix_next, flags & (set_mask | dirbit));
							int ret = solve_rec(depthat+1, maxdepth);
							if (ret == -1) n_solvable = -99;
							if (ret == 0)
							{
								set_state(ix, state[ix] & ~dirbit);
								didsomething = true;
								
								if (!fill_known(depthat)) return 0;
								if (finished(depthat)) return 1;
							}
							if (ret == 1) n_solvable++;
							if (ret == 2) return 2;
						}
					}
					if (n_solvable > 1) return 2;
					
					if (!(state[ix] & (0x0111<<dir))) return 0; // all options are impossible
				}
			}
		}
		
		if (didsomething) goto fill_again;
		
		return -1;
	}
	
public:
	
	//Takes a string of the form " 2 \n271\n" 2 \n". Calculates width and height automatically.
	//Must be rectangular; trailing spaces must exist on all lines, and a trailing linebreak must exist. Non-square inputs are fine.
	void init(cstring inmap)
	{
		width = 0;
		while (inmap[width] != '\n') width++;
		height = 0;
		while (inmap[(width+1) * height] != '\0') height++;
		size = height * width;
		
		map.resize(size);
		state.resize(size);
		
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
	
	//maxdepth is number of nested guesses may be made to solve the map.
	//Runtime is O(s^(4(d+1))), where s is width*height and d is max depth. (Probably less, but I can't prove it.)
	//Higher depth can solve more maps, but maps needing high depth are also hard to solve for humans.
	//Returns:
	//-1 if it's unsolvable with that max depth
	//0 if unsolvable
	//1 if there is a unique solution
	//2 if there are multiple valid solutions
	//If 1, you may call solution().
	int solve(int maxdepth = 3)
	{
		state.reserve(size*(maxdepth+1));
		for (int depth = 0; depth <= maxdepth; depth++)
		{
			int ret = solve_rec(0, depth);
			if (ret >= 0) return ret;
		}
		return -1;
	}
	
	//The solution is in the same shape as the input, and contains the number of bridges that exit at the right of the given cell.
	//Ocean tiles are spaces.
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
				if (flags & (flags>>4 | flags>>8)) return "";
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
};

static void test_split(cstring in, string& map, string& solution)
{
	array<cstring> lines = in.csplit("\n");
	
	for (size_t i=0;i<lines.size()-1;i+=2)
	{
		map      += lines[i  ]+"\n";
		solution += lines[i+1]+"\n";
	}
}

static void test_one(int depth, cstring test)
{
	string map;
	string solution;
	test_split(test, map, solution);
	
	solver s;
	s.init(map);
	assert_eq(s.solve(2), 1);
	assert_eq(s.solution(), solution);
}

static void test_one(cstring test)
{
	return test_one(2, test);
}

static void test_error(int depth, int exp, cstring map)
{
	solver s;
	s.init(map);
	assert_eq(s.solve(depth), exp);
}

static void test_error(int exp, cstring map)
{
	return test_error(2, exp, map);
}

static void test_multistep(cstring test)
{
	string map;
	string solution;
	test_split(test, map, solution);
	
	solver s;
	s.init(map);
	assert_eq(s.solve(1), -1);
	assert_eq(s.solve(2), 1);
//puts(s.solution());
	assert_eq(s.solution(), solution);
}

test("solver", "", "solver")
{
	testcall(test_one( // just something simple
		" 2 \n" /* */ " 0 \n"
		"271\n" /* */ "210\n"
		" 2 \n" /* */ " 0 \n"
	));
	testcall(test_one( // requires multiple passes
		" 1  3\n" /* */ " 1  0\n"
		"2 1  \n" /* */ "1 0  \n"
		"2  23\n" /* */ "1  10\n"
	));
	testcall(test_one( // bridges may not cross
		"464\n" /* */ "220\n"
		"4 4\n" /* */ "0 0\n"
		"2 2\n" /* */ "0 0\n"
		" 2 \n" /* */ " 0 \n"
	));
	testcall(test_one( // requires guessing
		"2 2\n" /* */ "1 0\n"
		"222\n" /* */ "110\n"
	));
	testcall(test_one( // all islands must be connected
		"22\n" /* */ "10\n"
		"22\n" /* */ "10\n"
	));
	testcall(test_one( // these cycles may confuse the island tracker
		"1 2  \n" /* */ "1 0  \n"
		" 1 1 \n" /* */ " 0 0 \n"
		"14441\n" /* */ "11110\n"
		" 1 1 \n" /* */ " 0 0 \n"
		"  2 1\n" /* */ "  1 0\n"
	));
	testcall(test_one( // a few cycles, to confuse the island tracker
		"4    4 \n" /* */ "2    0 \n"
		"  4 4  \n" /* */ "  2 0  \n"
		"    682\n" /* */ "    220\n"
		" 282   \n" /* */ " 220   \n"
		"  4 4  \n" /* */ "  2 0  \n"
		"4    4 \n" /* */ "2    0 \n"
	));
	testcall(test_one( // also requires guessing, multiple times (TODO: find a map that requires nested guesses)
		"2  2\n" /* */ "1  0\n"
		" 22 \n" /* */ " 10 \n"
		"  33\n" /* */ "  10\n"
		" 22 \n" /* */ " 10 \n"
		"2  2\n" /* */ "1  0\n"
	));
	testcall(test_one( // nasty amount of cycles because must annoy island tracker
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
	testcall(test_error(2, // has multiple solutions
		"33\n"
		"22\n"
	));
	testcall(test_error(1, -1, // this too - must not return 1 for that map
		" 31\n"
		"272\n"
		"131\n"
	));
	testcall(test_error(2, 2,
		" 31\n"
		"272\n"
		"131\n"
	));
	
	// these require two steps (created by the game generator)
	testcall(test_multistep(
		"2  1\n" /* */ "0  0\n"
		"3233\n" /* */ "1110\n"
		" 131\n" /* */ " 100\n"
		"1 2 \n" /* */ "1 0 \n"
	));
	testcall(test_multistep(
		"1232\n" /* */ "0120\n"
		"2 11\n" /* */ "0 00\n"
		"5543\n" /* */ "2120\n"
		"23 2\n" /* */ "02 0\n"
	));
	testcall(test_multistep(
		"1332\n" /* */ "1100\n"
		"13 3\n" /* */ "10 0\n"
		"21  \n" /* */ "00  \n"
		"3 42\n" /* */ "1 10\n"
	));
	testcall(test_multistep(
		"3431\n" /* */ "2110\n"
		" 112\n" /* */ " 000\n"
		"32 3\n" /* */ "11 0\n"
		"2  1\n" /* */ "1  0\n"
	));
}


class generator {
	static string generate_blank(int width, int height)
	{
		string line = "";
		for (int i=0;i<width;i++) line+=' ';
		line += "\n";
		
		string ret = "";
		for (int i=0;i<height;i++) ret += line;
		return ret;
	}
	
	static void fill_until(string& ret, int width, int y, int x, int dir, char replacement, int add = 0)
	{
		int ix = (width+1)*y + x;
		int offsets[4] = { 1, -(width+1), -1, width+1 };
		int offset = offsets[dir];
		
		while (true)
		{
			ix += offset;
			if (ix < 0 || ix >= (int)ret.length() || ret[ix] == '\n') break;
			if (isdigit(ret[ix]))
			{
				ret[ix] += add;
				break;
			}
			if (replacement!='#' && ret[ix]!=' ') break;
			ret[ix] = replacement;
		}
	}
	
	static void add_island(string& ret, int width, int y, int x)
	{
		int nbridge = 1 + rand()%2;
		switch (ret[(width+1)*y + x])
		{
			case '>':
				fill_until(ret, width, y, x, 0, '#', nbridge);
				break;
			case '^':
				fill_until(ret, width, y, x, 1, '#', nbridge);
				break;
			case '<':
				fill_until(ret, width, y, x, 2, '#', nbridge);
				break;
			case 'v':
				fill_until(ret, width, y, x, 3, '#', nbridge);
				break;
			case ' ':
				nbridge = 0;
				break;
		}
		ret[(width+1)*y + x] = '0'+nbridge;
		
		fill_until(ret, width, y, x, 0, '<');
		fill_until(ret, width, y, x, 1, 'v');
		fill_until(ret, width, y, x, 2, '>');
		fill_until(ret, width, y, x, 3, '^');
	}
	
public:
	static string generate(int width, int height, int islands)
	{
		string ret = generate_blank(width, height);
		int x = rand()%width;
		int y = rand()%height;
		
		//possible values in the string:
		//space - empty
		//0-8 - number of bridges
		//# - the intended solution has a bridge here
		//>^<v - can place island here, bridge is in the given direction
		//\n - line separator
		
		add_island(ret, width, y, x);
		islands--;
		
		while (islands)
		{
			int nslots = 0;
			int nix = 0;
			for (unsigned ix=0;ix<ret.length();ix++)
			{
				if (strchr(">^<v", ret[ix]))
				{
					nslots++;
					if (rand() % nslots == 0) nix = ix;
				}
			}
			
			if (nslots != 0) add_island(ret, width, nix/(width+1), nix%(width+1));
			
			islands--;
		}
		
		for (unsigned ix=0;ix<ret.length();ix++)
		{
			if (strchr(">^<v#", ret[ix])) ret[ix] = ' ';
		}
		
		return ret;
	}
};

#include <valgrind/valgrind.h>

int main(int argc, char** argv)
{
	arlib_init(NULL, argv);
srand(0);
	bool vg = RUNNING_ON_VALGRIND;
	
	int nsolv = 0;
	int nunsolv = 0;
	int nmultisolv = 0;
	for (int side=3;side<8;side++)
	{
		for (int i=0;i<(vg ? 1000 : 10000);i++)
		{
			if (i%(vg ? 100 : 1000) == 0) { printf("%i %i     \r", side, i); fflush(stdout); }
			int minislands = side*2;
			int maxislands = side*side/10;
			int islands = minislands + rand()%(maxislands+1-minislands);
			string map = generator::generate(side, side, islands);
//puts(map);
			
			solver s;
			s.init(map);
			int steps;
			int result;
			for (steps = 0; true; steps++)
			{
//if(steps>=3)printf("%i %i     \n", side, steps); fflush(stdout);
				result = s.solve(steps);
				if (result != -1) break;
			}
			if (steps >= 3 && result == 1)
			//if (steps >= 5)
			{
				puts("MULTISTEP "+tostring(steps)+" "+tostring(result));
				puts(map);
			}
			if (result == 0)
			{
				puts("ERROR");
				puts(map);
				exit(0);
			}
			if (result == -1) nunsolv++;
			if (result == 0) nunsolv++;
			if (result == 1) nsolv++;
			if (result == 2) nmultisolv++;
//break;
		}
//break;
	}
	printf("%i solvable, %i unsolvable, %i multiple solutions\n", nsolv, nunsolv, nmultisolv);
	
	return 0;
}
