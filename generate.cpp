#include "game.h"
#include <math.h>

//During generate_one, towalk is
// 0 if there can't be any island here because it wouldn't connect to anything (or would be too long)
// 1 if it may be possible to place an island here
// 2 if that's right beside an island and allow_dense is false
// 3 if that's an island, bridge or reef
// 80..83 if that's an island or bridge, and it's connected to a castle of the given color

namespace {
class loc_generator {
public:

gamemap& map;
const gamemap::genparams& par;

//use a per-thread RNG, to avoid thread safety issues (or, if rand() is mutexed, avoid the associated cacheline bouncing)
//I could've used rand_r instead, but that'd require an ifdef on windows and I'd rather not.
random_t rand;

loc_generator(gamemap& map, const gamemap::genparams& par) : map(map), par(par) {}

uint16_t valid_bridges_from(int x, int y, bool doit, uint16_t& color)
{
	int maxbrilen = 4;
	
	int valid_dirs = 0;
	uint16_t myroot = map.map[y][x].rootnode;
	int8_t bridgemark;
	
	int n_colors = 0;
	
	//right
	bridgemark = (doit ? maxbrilen : 0);
	for (int xx=x+1;xx<map.width && xx<=x+maxbrilen;xx++)
	{
		uint16_t index = y*100 + xx;
		if ((bridgemark-- > 0) && map.towalk[index] == 0)
			map.towalk[index] = 1;
		if (map.towalk[index] >= 3)
		{
			if (map.map[y][xx].rootnode == myroot) break; // can't connect to self
			if (map.map[y][xx].population < 0) break; // can't connect to crossing bridge or reef
			if (color != (uint16_t)-1 && map.towalk[index] != color) break; // can't connect to a castle of another color
			
			if (rand() % ++n_colors == 0) color = map.towalk[index];
			valid_dirs |= 1<<0;
			break;
		}
	}
	
	//up
	bridgemark = (doit ? maxbrilen : 0);
	for (int yy=y-1;yy>=0 && yy>=y-maxbrilen;yy--)
	{
		uint16_t index = yy*100 + x;
		if ((bridgemark-- > 0) && map.towalk[index] == 0)
			map.towalk[index] = 1;
		if (map.towalk[index] >= 3)
		{
			if (map.map[yy][x].rootnode == myroot) break;
			if (map.map[yy][x].population < 0) break;
			if (color != (uint16_t)-1 && map.towalk[index] != color) break;
			
			if (rand() % ++n_colors == 0) color = map.towalk[index];
			valid_dirs |= 1<<1;
			break;
		}
	}
	
	//left
	bridgemark = (doit ? maxbrilen : 0);
	for (int xx=x-1;xx>=0 && xx>=x-maxbrilen;xx--)
	{
		uint16_t index = y*100 + xx;
		if ((bridgemark-- > 0) && map.towalk[index] == 0)
			map.towalk[index] = 1;
		if (map.towalk[index] >= 3)
		{
			if (map.map[y][xx].rootnode == myroot) break;
			if (map.map[y][xx].population < 0) break;
			if (color != (uint16_t)-1 && map.towalk[index] != color) break;
			
			if (rand() % ++n_colors == 0) color = map.towalk[index];
			valid_dirs |= 1<<2;
			break;
		}
	}
	
	//down
	bridgemark = (doit ? maxbrilen : 0);
	for (int yy=y+1;yy<map.height && yy<=y+maxbrilen;yy++)
	{
		uint16_t index = yy*100 + x;
		if ((bridgemark-- > 0) && map.towalk[index] == 0)
			map.towalk[index] = 1;
		if (map.towalk[index] >= 3)
		{
			if (map.map[yy][x].rootnode == myroot) break;
			if (map.map[yy][x].population < 0) break;
			if (color != (uint16_t)-1 && map.towalk[index] != color) break;
			
			if (rand() % ++n_colors == 0) color = map.towalk[index];
			valid_dirs |= 1<<3;
			break;
		}
	}
//printf("VALIDFROM[%.3i]=%X\n",y*100+x,valid_dirs);
	
	return valid_dirs;
}

//joined, if nonzero, will try to make a large island containing up to 'joined' additional tiles
//may make a smaller one, and may choose to not make a large island at all
bool add_island_sub(int x, int y, uint16_t root, bool force, uint16_t color)
{
	gamemap::island& here = map.map[y][x];
	here.population = 0;
	here.rootnode = root;
	map.numislands++;
	
	int valid_dirs = valid_bridges_from(x, y, true, color);
	map.towalk[y*100+x] = color;
	
	bool any = false;
	do {
		for (int dir=0;dir<4;dir++)
		{
			if (!(valid_dirs & (1<<dir))) continue;
			if (rand()%100 >= 50) continue;
			
			static const int offsets[4] = { 1, -100, -1, 100 };
			
			int8_t len = 0;
			while (map.get(y*100+x + (++len)*offsets[dir]).population < 0) {}
			if (rand()%(len*100) >= 250) continue; // reduce probability of long bridges
			
//printf("MAKE: SRC=[%.3i]=%i, DST=[%.3i]=%.3i\n", y*100+x,color,y*100+x + len*offsets[dir],map.towalk[y*100+x + len*offsets[dir]]);
			if (map.towalk[y*100+x + len*offsets[dir]] != color) abort(); // valid_bridges_from should ban this
			
			int nbri = rand()%2 + 1;
			
			if (map.get(root).population < 80)
			{
				map.get(root).population += nbri;
			}
			here.bridges[dir] = nbri;
			here.bridgelen[dir] = len;
			
			int idx = y*100+x;
			do {
				idx += offsets[dir];
				map.towalk[idx] = color;
			} while (map.get(idx).population < 0);
			
			if (map.get(map.get(idx).rootnode).population < 80)
			{
				map.get(map.get(idx).rootnode).population += nbri;
			}
			map.get(idx).bridges[dir^2] = nbri;
			map.get(idx).bridgelen[dir^2] = len;
			
			any = true;
		}
	} while (force && !any);
	
	if (!par.allow_dense)
	{
		if (x<map.width-1  && map.towalk[y*100+x+1  ]<=1) map.towalk[y*100+x+1  ]=2;
		if (y<map.height-1 && map.towalk[y*100+x+100]<=1) map.towalk[y*100+x+100]=2;
		if (x>0            && map.towalk[y*100+x-1  ]<=1) map.towalk[y*100+x-1  ]=2;
		if (y>0            && map.towalk[y*100+x-100]<=1) map.towalk[y*100+x-100]=2;
	}
	
	return true;
}

bool try_add_island_sub(int x, int y, uint16_t root, bool force, uint16_t color)
{
	if (x<0 || y<0 || x>=map.width || y>=map.height || map.towalk[y*100+x] >= 3) return false;
	return add_island_sub(x, y, root, force, color);
}

bool try_add_island(int x, int y)
{
	uint16_t root = y*100+x;
	if (map.towalk[root] != 1) return false; // can be removed for debugging, but never remove the next one
	if (map.towalk[root] > 1) return false;
	
	uint16_t color = -1;
	if (!valid_bridges_from(x, y, false, color)) return false;
	
	//int forcedir = valid_bridges_from(map, par, x, y);
	add_island_sub(x, y, root, true, color);
	
	//all two- and three-tile large islands are valid, as are 2x2 squares
	if (par.use_large && rand()%100<50)
	{
		switch (rand()%10)
		{
#define X(xx,yy) try_add_island_sub(x+xx, y+yy, root, false, color)
		case 0: // right
			X(+1,0) && X(+2,0);
			break;
		case 1: // left
			X(-1,0) && X(-2,0);
			break;
		case 2: // horz centered
			X(+1,0); X(-1,0);
			break;
		case 3: // up
			X(0,-1) && X(0,-2);
			break;
		case 4: // down
			X(0,+1) && X(0,+2);
			break;
		case 5: // vert centered
			X(0,-1); X(0,+1);
			break;
		case 6: // square, up left
			(X(-1,0) + X(0,-1)) && X(-1,-1);
			break;
		case 7: // square, up right
			(X(+1,0) + X(0,-1)) && X(+1,-1);
			break;
		case 8: // square, down left
			(X(-1,0) + X(0,+1)) && X(-1,+1);
			break;
		case 9: // square, down right
			(X(+1,0) + X(0,+1)) && X(+1,+1);
			break;
#undef X
		}
	}
	
	return true;
}

static uint32_t gcd(uint32_t a, uint32_t b)
{
	if (b == 0) return a;
	else return gcd(b, a%b);
}

//from https://stackoverflow.com/questions/6822628/random-number-relatively-prime-to-an-input
//probably doesn't give all valid answers with equal probability, but good enough
uint32_t rand_coprime(uint32_t to)
{
	uint32_t ret = rand()%(to-1) + 1;
	uint32_t tmp;
	do {
		tmp = gcd(ret, to);
		ret /= tmp;
	} while (tmp > 1);
	return ret;
}

//Ignores allow_multi, difficulty and quality.
void generate_one(uint64_t seed)
{
	rand.seed(seed);
	
	map.width = par.width;
	map.height = par.height;
	map.numislands = 0;
	
	for (int y=0;y<map.height;y++)
	for (int x=0;x<map.width;x++)
	{
		gamemap::island& here = map.map[y][x];
		
		here.population = -1;
		here.totbridges = 0;
		here.rootnode = y*100+x;
		
		here.bridgelen[0] = -1;
		here.bridgelen[1] = -1;
		here.bridgelen[2] = -1;
		here.bridgelen[3] = -1;
		
		here.bridges[0] = 0;
		here.bridges[1] = 0;
		here.bridges[2] = 0;
		here.bridges[3] = 0;
		
		map.towalk[y*100+x] = 0;
	}
	
	map.an_island[1] = -1;
	map.an_island[2] = -1;
	map.an_island[3] = -1;
	map.an_island[4] = -1;
	
	if (!par.use_castle)
	{
		int startx = rand() % par.width;
		int starty = rand() % par.height;
		map.an_island[0] = starty*100 + startx;
		add_island_sub(startx, starty, starty*100+startx, false, 3);
		
		map.has_castles = false;
	}
	int ncastle = 0;
	int ncastleeach = 1;
	if (par.use_castle)
	{
		ncastle = rand()%3 + 2;
		uint8_t colors[4] = {80,81,82,83};
		arrayvieww<uint8_t>(colors).shuffle();
		map.has_castles = true;
		
		//minimum sizes with 2 castles, such that there's always a place to put the last castle,
		// no matter where the first ones ended up, even with allow_dense false:
		//2x? - 2x7
		//3x? - 3x7
		//4x? - 4x7
		//5x? - 5x5
		//6x? - 6x5
		//
		//minimum sizes with 3 castles:
		//2x? - 2x12
		//3x? - 3x12
		//4x? - 4x12
		//5x? - 5x9
		//6x? - 6x8
		//7x? - 7x7
		//
		//minimum sizes with 4 castles:
		//2x? - 2x17
		//3x? - 3x17
		//4x? - 4x17
		//5x? - 5x13
		//6x? - 6x11
		//7x? - 7x7
		
		//-> allow 3 or 4 castles on 7x7 maps, but anything smaller gets 2
		if (par.width < 7 || par.height < 7) ncastle = 2;
		if (par.width < 3 || par.height < 3) ncastle = 1; // tiny maps get only one castle
		if (par.width < 5 && par.height < 5) ncastle = 1;
		
		
		for (int i=0;i<ncastle;i++)
		{
			while (true)
			{
				int x = rand() % (par.width-1);
				int y = rand() % (par.height-1);
				
				if (map.towalk[y*100+x    ] <= 1 &&
				    map.towalk[y*100+x+  1] <= 1 &&
				    map.towalk[y*100+x+100] <= 1 &&
				    map.towalk[y*100+x+101] <= 1)
				{
					add_island_sub(x,   y,   y*100+x, false, colors[i]);
					add_island_sub(x+1, y,   y*100+x, false, colors[i]);
					add_island_sub(x,   y+1, y*100+x, false, colors[i]);
					add_island_sub(x+1, y+1, y*100+x, false, colors[i]);
					
					map.map[y][x].population = colors[i];
					map.towalk[y*100+x] = colors[i];
					
					map.an_island[0] = y*100 + x;
					map.an_island[colors[i]-80+1] = y*100 + x;
					
					break;
				}
			}
		}
	}
	
	if (par.use_reef)
	{
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			if (rand()%100 > 3) continue;
			if (map.towalk[y*100+x] >= 3) continue;
			
			map.towalk[y*100+x] = 3;
			map.map[y][x].population = -2;
		}
	}
	
	int totislands = par.width*par.height*par.density;
	totislands = totislands * (rand()%100+50) / 100; // multiply by 50%..150%
	//make sure it doesn't loop forever saying "here's a map with density 0" "don't give me maps with only one island!"
	if (totislands < 4) totislands = 4;
	while (map.numislands < totislands)
	{
		//packed index - equal to y*width+x, as opposed to normal index (y*100+x)
		int pidx_start = rand() % (par.width*par.height);
		int pidx_skip = rand_coprime(par.width*par.height);
		
		int pidx = pidx_start;
		
		while (true)
		{
			uint16_t y = pidx / par.width;
			uint16_t x = pidx % par.width;
			
			if (try_add_island(x, y)) break;
			else if (map.towalk[y*100+x] == 1) map.towalk[y*100+x] = 0;
			
			pidx = (pidx+pidx_skip) % (par.width*par.height);
			if (pidx == pidx_start) goto done; // all possibilities taken already?
		}
		
		//try to create more castles
		if (par.use_castle && ncastle*ncastleeach <= 4)
		{
//puts(".");
			uint16_t numoptions[4] = { 0,0,0,0 };
			uint16_t newcastle[4];
			uint16_t newroot[4];
			
			for (int y=0;y<par.height-1;y++)
			for (int x=0;x<par.width-1;x++)
			{
				//if
				//- at least one tile is island
				//- all four tiles are available (towalk < 3, or rootnode same)
				//   (towalk=2 is always true somewhere unless the island initially was a 2x2,
				//     which is a pointless restriction, so it must be allowed)
				//- none of the eight surrounding tiles has the same root node
				//- allow_multi is true, or this one is not beside a castle of the same color
				//then a castle can be placed here
				
				uint16_t index = y*100+x;
				
				//- at least one tile is island
				uint16_t root = -1;
				if (map.get(index    ).population >= 0) root = index    ;
				if (map.get(index+  1).population >= 0) root = index+  1;
				if (map.get(index+100).population >= 0) root = index+100;
				if (map.get(index+101).population >= 0) root = index+101;
				
				if (root == (uint16_t)-1) continue;
				root = map.get(root).rootnode;
				if (map.get(root).population >= 80) continue;
				
				//- all four tiles are available (towalk < 3)
				if (map.towalk[index    ] >= 3 && map.get(index    ).rootnode != root) continue;
				if (map.towalk[index+  1] >= 3 && map.get(index+  1).rootnode != root) continue;
				if (map.towalk[index+100] >= 3 && map.get(index+100).rootnode != root) continue;
				if (map.towalk[index+101] >= 3 && map.get(index+101).rootnode != root) continue;
				
				//- all four tiles are available (rootnode same)
				if (map.get(index    ).population >= 0 && map.get(index    ).rootnode != root) continue;
				if (map.get(index+  1).population >= 0 && map.get(index+  1).rootnode != root) continue;
				if (map.get(index+100).population >= 0 && map.get(index+100).rootnode != root) continue;
				if (map.get(index+101).population >= 0 && map.get(index+101).rootnode != root) continue;
				
				//- none of the eight surrounding tiles has the same root node
				if (x > 0 && map.get(index    -1).population >= 0 && map.get(index    -1).rootnode == root) continue;
				if (x > 0 && map.get(index+100-1).population >= 0 && map.get(index+100-1).rootnode == root) continue;
				if (y > 0 && map.get(index-100  ).population >= 0 && map.get(index-100  ).rootnode == root) continue;
				if (y > 0 && map.get(index-100+1).population >= 0 && map.get(index-100+1).rootnode == root) continue;
				
				//- none of the eight surrounding tiles has the same root node
				if (x < map.width-1  && map.get(index+  2).population >= 0 && map.get(index+  2).rootnode == root) continue;
				if (x < map.width-1  && map.get(index+102).population >= 0 && map.get(index+102).rootnode == root) continue;
				if (y < map.height-1 && map.get(index+200).population >= 0 && map.get(index+200).rootnode == root) continue;
				if (y < map.height-1 && map.get(index+201).population >= 0 && map.get(index+201).rootnode == root) continue;
				
				//- allow_multi is true, or this one is not beside a castle of the same color
				//(TODO: allow, but force zero bridges that way)
				if (!par.allow_multi)
				{
					for (int tile=0;tile<4;tile++)
					{
						static const int8_t tilepos[] = { 0, 1, 100, 101 };
						uint16_t lindex = index + tilepos[tile];
						
						static const int8_t diroff[] = { 1, -100, -1, 100 };
						for (int dir=0;dir<4;dir++)
						{
							int8_t bridgelen = map.get(lindex).bridgelen[dir];
							if (bridgelen == -1) continue;
							
							uint16_t newindex = lindex + bridgelen*diroff[dir];
							//printf("%.4X\n",newindex);
							if (map.get(newindex).population == map.towalk[root]) goto has_castle_link;
						}
					}
				}
				if (false) { has_castle_link: continue; }
				
				uint16_t color = map.towalk[root];
//printf("CASTLEVALID[%i]:%.3i,%.3i\n",color,root,index);
				if (rand()%(++numoptions[color-80]) != 0) continue;
				
				newcastle[color-80] = index;
				newroot[color-80] = root;
			}
			
			if (!!numoptions[0] + !!numoptions[1] + !!numoptions[2] + !!numoptions[3] == ncastle)
			{
				//make sure the new islands don't overlap
				for (int i=0;i<4;i++)
				{
					for (int j=0;j<4;j++)
					{
						if (numoptions[i] == 0) continue;
						if (numoptions[j] == 0) continue;
						
						uint16_t pos1 = newcastle[i];
						uint16_t pos2 = newcastle[j];
						
						if (pos1 == pos2 + 001) goto skip_new_castles;
						if (pos1 == pos2 + 101) goto skip_new_castles;
						if (pos1 == pos2 + 100) goto skip_new_castles;
						if (pos1 == pos2 +  99) goto skip_new_castles;
						//these four checks are enough; if they overlap in other directions, it'll be one of those for the other one
						//equal position is impossible, it'd cause unauthorized overlaps
					}
				}
				
				ncastleeach++;
				for (int i=0;i<4;i++)
				{
					uint16_t pos = newcastle[i];
//printf("CASTLE[%i]=%.3i\n",i,pos);
					if (numoptions[i] == 0) continue;
					
					map.towalk[pos    ] = 80+i; // preallocate these so it won't try to build bridges across them
					map.towalk[pos+  1] = 80+i;
					map.towalk[pos+100] = 80+i;
					map.towalk[pos+101] = 80+i;
				}
				for (int i=0;i<4;i++)
				{
					uint16_t pos = newcastle[i];
					if (numoptions[i] == 0) continue;
					
					if (map.get(pos    ).population < 0) add_island_sub(pos%100  , pos/100  , newroot[i], false, 80+i);
					if (map.get(pos+  1).population < 0) add_island_sub(pos%100+1, pos/100  , newroot[i], false, 80+i);
					if (map.get(pos+100).population < 0) add_island_sub(pos%100  , pos/100+1, newroot[i], false, 80+i);
					if (map.get(pos+101).population < 0) add_island_sub(pos%100+1, pos/100+1, newroot[i], false, 80+i);
					
					map.get(newroot[i]).population = 80+i;
				}
			skip_new_castles: ;
			}
//puts(".");
		}
//for (int y=0;y<map.height;y++)
//for (int x=0;x<map.width;x++)
//{
//if(y&&!x)puts("");
//printf("%i",map.towalk[y*100+x]%(80-6));
//}
//puts("\n");
//for (int y=0;y<map.height;y++)
//for (int x=0;x<map.width;x++)
//{
//if(y&&!x)puts("");
//char ch=map.map[y][x].population;
//if(0);
//else if(ch<0)ch='?';
//else if(ch<10)ch='0'+ch;
//else if(ch<20)ch='A'+ch-10;
//else ch="rbyg"[ch-80];
//printf("%c",ch);
//}
//puts("\n\n");
	}
done: ;
	
	//to ensure large islands' roots are not at bottom right, and its location is not a clue:
	//- set towalk on each island to point to itself
	//- for every tile, swap towalk with its root; this creates linked lists
	//- for each large island, pick a random valid root, then use it
	for (int y=0;y<map.height;y++)
	for (int x=0;x<map.width;x++)
	{
		map.towalk[y*100+x] = y*100+x;
	}
	
	for (int y=0;y<map.height;y++)
	for (int x=0;x<map.width;x++)
	{
		gamemap::island& here = map.map[y][x];
		if (here.population < 0) continue;
		
		uint16_t swap = map.towalk[y*100+x];
		map.towalk[y*100+x] = map.towalk[here.rootnode];
		map.towalk[here.rootnode] = swap;
		
		//also repair bridgelen, bridges==3, non-root population, and bridge counts for oceans
		here.population = map.get(here.rootnode).population;
		here.totbridges = (here.population < 80 ? here.population : 16); // the exact number doesn't matter
		
		for (int x2=x-1;x2>=0;x2--)
		{
			if (map.map[y][x2].population != -1)
			{
				if (map.map[y][x2].population == -2) break;
				
				here.bridgelen[2] = x-x2;
				map.map[y][x2].bridgelen[0] = x-x2;
				for (int x3=x2+1;x3<x;x3++)
				{
					map.map[y][x3].bridgelen[0] = x-x3;
					map.map[y][x3].bridgelen[2] = x3-x2;
					map.map[y][x3].bridges[0] = here.bridges[2];
					map.map[y][x3].bridges[2] = here.bridges[2];
				}
				
				if (map.map[y][x2].rootnode == here.rootnode && x-x2==1)
				{
					here.bridges[2] = 3;
					map.map[y][x2].bridges[0] = 3;
				}
				
				break;
			}
		}
		for (int y2=y-1;y2>=0;y2--)
		{
			if (map.map[y2][x].population != -1)
			{
				if (map.map[y2][x].population == -2) break;
				
				here.bridgelen[1] = y-y2;
				map.map[y2][x].bridgelen[3] = y-y2;
				for (int y3=y2+1;y3<y;y3++)
				{
					map.map[y3][x].bridgelen[1] = y3-y2;
					map.map[y3][x].bridgelen[3] = y-y3;
					map.map[y3][x].bridges[1] = here.bridges[1];
					map.map[y3][x].bridges[3] = here.bridges[1];
				}
				
				if (map.map[y2][x].rootnode == here.rootnode && y-y2==1)
				{
					here.bridges[1] = 3;
					map.map[y2][x].bridges[3] = 3;
				}
				
				break;
			}
		}
	}
	
	for (int y=0;y<map.height;y++)
	for (int x=0;x<map.width;x++)
	{
		int idx = y*100+x;
		if (map.map[y][x].population < 0) continue;  // ignore oceans
		if (map.towalk[idx]==idx) continue;          // ignore small islands
		// I could ignore non-roots, but then I'd rerandomize the root if it moved down. I'd rather not bias it.
		// TODO: examine whether there's some interesting mathematical property on anything in towalk,
		//  for example if the linked lists are strictly increasing except when it loops back to the lowest
		//if (map.map[y][x].rootnode != idx) continue;
		
		int newroot = -1; // a valid root is known to exist, but gcc won't realize that
		int nroots = 0;
		do {
			gamemap::island& here = map.get(idx);
			
			bool valid_root = false;
			
			if (here.population >= 80)
			{
				//castles are always on a 2x2, so this always works, even if it doesn't check that bottom-right is connected
				valid_root = (here.bridges[0]==3 && here.bridges[3]==3);
			}
			else
			{
				valid_root = (here.bridges[0]==3 || here.bridges[3]==3);
			}
			
			if (valid_root)
			{
				nroots++;
				if (rand()%nroots == 0) newroot = idx;
			}
			idx = map.towalk[idx];
		} while (idx != y*100+x);
		
		do {
			map.get(idx).rootnode = newroot;
			idx = map.towalk[idx];
		} while (idx != y*100+x);
	}
}
};
}


gamemap::generator::generator(const gamemap::genparams& par) : par(par)
{
	//WARNING: Even if this one is made constant, output will still be nondeterministic,
	//  due to threads finishing in varying order, making different maps seem superior;
	//  therefore, it is not allowed to override it.
	//A fixed random seed with exactly one thread will be deterministic.
	randseed = time_us() << 16;
	
#ifdef ARLIB_THREAD
	unsigned n_threads = thread_num_cores_idle();
	// don't spin up too many shortlived threads
	if (n_threads > par.quality / 500) n_threads = par.quality / 500;
	if (n_threads > 32) n_threads = 32; // more than 32 is just excessive, such high quality is pointless
	if (n_threads < 1) n_threads = 1;
	
	n_started = n_threads;
	n_more_threads = n_threads-1;
	thread_create(bind_this(&generator::threadproc), pri_idle);
#else
	n_started = 1;
#endif
}

//If 'first', the thread is freshly started, and has already claimed its work via n_started.
//If false, there's no more work to do; thread should terminate.
//Must be called while holding the mutex.
bool gamemap::generator::get_work(bool first, workitem& w)
{
	if (!first)
	{
		if (stop) return false;
		if (n_valid > 0 && n_started >= par.quality) return false;
		n_started++;
	}
	
	randseed++;
	w.seed = randseed;
	return true;
}
//Should be called while not holding the mutex.
void gamemap::generator::do_work(workitem& w, gamemap& map)
{
	loc_generator gen(map, par);
	gen.generate_one(w.seed);
	
	//if (!map.finished()) abort();
	w.valid = true;
	if (map.numislands <= 1 && par.width*par.height >= 5) w.valid = false;
	if (!par.allow_multi && map.solve_another()) w.valid = false;
	
	if (w.valid) w.diff = map.difficulty();
}
//Must be called while holding the mutex.
void gamemap::generator::finish_work(workitem& w)
{
	n_finished++;
	
	if (w.valid)
	{
		n_valid++;
		
		if (w.diff > diff_max) diff_max = w.diff;
		if (w.diff < diff_min) diff_min = w.diff;
		
		//these two can be NaN or negative if only one difficulty has been observed, but then diff_max<=diff_min and the NaNs aren't used
		float diff_scale = (w.diff-diff_min) / (float)(diff_max-diff_min);
		float best_diff_scale = (best_diff-diff_min) / (float)(diff_max-diff_min);
		if (diff_max<=diff_min || fabs(diff_scale - par.difficulty) <= fabs(best_diff_scale - par.difficulty))
		{
			best_diff = w.diff;
			best_seed = w.seed;
		}
	}
}

void gamemap::generator::threadproc()
{
	gamemap map; // this object is huge, cache it to avoid pointless stack probing
	
	workitem w;
	synchronized(mut)
	{
		get_work(true, w);
		
		if (n_more_threads)
		{
			thread_create(bind_this(&generator::threadproc), pri_idle);
			n_more_threads--;
		}
	}
	
	while (true)
	{
		do_work(w, map);
		
		synchronized(mut)
		{
			finish_work(w);
			
			if (!get_work(false, w))
			{
				if (n_started == n_finished) sem.release();
				return; // not break, synchronized() uses a for loop internally
			}
		}
	}
}

bool gamemap::generator::done(unsigned* progress)
{
#ifdef ARLIB_THREAD
	synchronized(mut)
	{
		if (progress) *progress = n_finished;
		return n_finished == n_started;
	}
	return false; // unreachable, but gcc doesn't know that
#else
	if (progress) *progress = par.quality;
	return true;
#endif
}

bool gamemap::generator::finish(gamemap& map)
{
	uint64_t seed = pack();
	if (!seed) return false;
	unpack(par, seed, map);
	return true;
}

uint64_t gamemap::generator::pack()
{
#ifndef ARLIB_THREAD
	threadproc();
#endif
	sem.wait();
	
//printf("Generated %u maps (%u usable)\n", n_finished, n_valid);
//printf("Difficulty: %i (%f, desired %f), range %i-%i\n", best_diff,
//(best_diff-diff_min) / (float)(diff_max-diff_min), par.difficulty, diff_min, diff_max);
//puts(serialize());
	
	uint64_t ret;
	synchronized(mut) // must grab mutex after awaiting the semaphore, to avoid race condition on releasing vs freeing mutex
	{
		ret = n_valid ? best_seed : 0;
	}
	
	sem.release(); // in case some noob decides to call finish() multiple times
	return ret;
}

void gamemap::generator::unpack(const gamemap::genparams& par, uint64_t seed, gamemap& map)
{
	loc_generator lgen(map, par);
	lgen.generate_one(seed);
}


void gamemap::generator::cancel()
{
	synchronized(mut) { stop = true; }
	sem.wait();
	sem.release(); // in case some noob decides to call finish() after this
}


test("generator","solver","generator")
{
//assert(0);
	for (int i=0;true;i++)
	{
//srand(i);
//printf("EEE=%i\n",i);
		if (i == 1000) assert(!"game generator needs to contain loops in its intended solutions");
		gamemap m;
		gamemap::genparams p = {};
		p.width=2;
		p.height=2;
		p.allow_dense=true;
		p.density=1.0;
		p.quality=1;
		m.generate(p);
		assert(m.finished());
		m.reset();
		assert(m.solve());
		
		if (m.map[0][0].bridges[0] && m.map[0][0].bridges[3] &&
		    m.map[1][0].bridges[0] && m.map[0][1].bridges[3])
		{
			break;
		}
	}
	
	{
		gamemap m;
		gamemap::genparams p = {};
		p.width=2;
		p.height=2;
		p.density=1.0;
		p.quality=1;
		m.generate(p);
		
		assert_eq(m.numislands, 1); // with allow_dense=false, only one island can fit
	}
}
