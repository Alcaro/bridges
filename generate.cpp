#include "game.h"
#include <math.h>

//During generate_one, towalk is
// 0 if there can't be any island here because it wouldn't connect to anything (or would be too long)
// 1 if it may be possible to place an island here
// 2 if that's right beside an island and allow_dense is false
// 3 if that's an island or bridge
// 80..83 if that's an island or bridge, and it's connected to a castle of the given color

//TODO: to add multiple castles of the same color, try to grow islands at some random point
//       make sure new castles don't overlap, even if two islands have to grow towards each other

static uint16_t valid_bridges_from(gamemap& map, const gamemap::genparams& par, int x, int y, bool doit, uint16_t& color)
{
	int maxbrilen = par.max_brilen;
	
	int valid_dirs = 0;
	uint16_t myroot = map.map[y][x].rootnode;
	int8_t bridgemark;
	
	int n_colors = 0;
	
	//right
	bridgemark = (doit ? 5 : 0);
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
	bridgemark = (doit ? 5 : 0);
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
	bridgemark = (doit ? 5 : 0);
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
	bridgemark = (doit ? 5 : 0);
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
static bool add_island_sub(gamemap& map, const gamemap::genparams& par, int x, int y, uint16_t root, bool force, uint16_t color)
{
	gamemap::island& here = map.map[y][x];
	here.population = 0;
	here.rootnode = root;
	map.numislands++;
	
	int valid_dirs = valid_bridges_from(map, par, x, y, true, color);
	map.towalk[y*100+x] = color;
	
	bool any = false;
	do {
		for (int dir=0;dir<4;dir++)
		{
			if (!(valid_dirs & (1<<dir))) continue;
			if (rand()%100 >= 50) continue;
			
			static const int offsets[4] = { 1, -100, -1, 100 };
			
			//reduce probability for long bridges
			int8_t len = 0;
			while (map.get(y*100+x + (++len)*offsets[dir]).population < 0) {}
			if (rand()%(len*100) >= 250) continue;
			
			if (map.towalk[y*100+x + len*offsets[dir]] != color) continue;
			
			int nbri = rand()%2 + 1;
			
			if (map.get(root).population < 80)
			{
				map.get(root).population += nbri;
			}
			here.bridges[dir] = nbri;
			
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

static bool try_add_island_sub(gamemap& map, const gamemap::genparams& par, int x, int y, uint16_t root, bool force, uint16_t color)
{
	if (x<0 || y<0 || x>=map.width || y>=map.height || map.towalk[y*100+x] >= 3) return false;
	return add_island_sub(map, par, x, y, root, force, color);
}

static bool try_add_island(gamemap& map, const gamemap::genparams& par, int x, int y)
{
	uint16_t root = y*100+x;
	if (map.towalk[root] != 1) return false; // can be removed for debugging, but never remove the next one
	if (map.towalk[root] > 1) return false;
	
	uint16_t color = -1;
	if (!valid_bridges_from(map, par, x, y, false, color)) return false;
	
	//int forcedir = valid_bridges_from(map, par, x, y);
	add_island_sub(map, par, x, y, root, true, color);
	
	//all two- and three-tile large islands are valid, as are 2x2 squares
	if (par.use_large && rand()%100<50)
	{
		switch (rand()%10)
		{
#define X(xx,yy) try_add_island_sub(map, par, x+xx, y+yy, root, false, color)
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
			(X(-1,0) || X(0,-1)) && X(-1,-1);
			break;
		case 7: // square, up right
			(X(+1,0) || X(0,-1)) && X(+1,-1);
			break;
		case 8: // square, down left
			(X(-1,0) || X(0,+1)) && X(-1,+1);
			break;
		case 9: // square, down right
			(X(+1,0) || X(0,+1)) && X(+1,+1);
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
static uint32_t rand_coprime(uint32_t to)
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
static void generate_one(gamemap& map, const gamemap::genparams& par)
{
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
		add_island_sub(map, par, startx, starty, starty*100+startx, false, 3);
		
		map.has_castles = false;
	}
	if (par.use_castle)
	{
		int ncastle = rand()%3 + 2;
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
					add_island_sub(map, par, x,   y,   y*100+x, false, colors[i]);
					add_island_sub(map, par, x+1, y,   y*100+x, false, colors[i]);
					add_island_sub(map, par, x,   y+1, y*100+x, false, colors[i]);
					add_island_sub(map, par, x+1, y+1, y*100+x, false, colors[i]);
					
					map.map[y][x].population = colors[i];
					map.towalk[y*100+x] = colors[i];
					
					map.an_island[0] = y*100 + x;
					map.an_island[colors[i]-80+1] = y*100 + x;
					
					break;
				}
			}
		}
	}
	
	int totislands = par.width*par.height*par.density;
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
			
			//uint16_t idx = y*100+x;
			if (try_add_island(map, par, x, y)) break;
			else if (map.towalk[y*100+x] == 1) map.towalk[y*100+x] = 0;
			
			pidx = (pidx+pidx_skip) % (par.width*par.height);
			if (pidx == pidx_start) goto done; // all possibilities taken already?
		}
//for (int y=0;y<map.height;y++)
//for (int x=0;x<map.width;x++)
//{
//if(y&&!x)puts("");
//printf("%i",map.towalk[y*100+x]);
//}
//puts("\n");
//for (int y=0;y<map.height;y++)
//for (int x=0;x<map.width;x++)
//{
//if(y&&!x)puts("");
//char ch=map.map[y][x].population;
//if(ch<0)ch='?';
//if(ch<10)ch='0'+ch;
//if(ch<20)ch='A'+ch-10;
//printf("%c",ch);
//}
//puts("\n\n");
	}
done: ;
	
	if (par.use_reef)
	{
		for (int y=0;y<map.height;y++)
		for (int x=0;x<map.width;x++)
		{
			if (map.towalk[y*100+x] < 3 && rand()%100<10)
				map.map[y][x].population = -2;
		}
	}
	
	//to make large islands' roots not be at bottom right, and ensure its location is not a clue:
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

void gamemap::generate(const genparams& par)
{
//static int i;srand(++i);printf("SEED=%i\n",i);
//srand(2);
	if (!par.max_brilen)
	{
		genparams newpar = par;
		newpar.max_brilen = (max(par.width, par.height)+2)/2.5;
		return generate(newpar);
	}
	
	int diff_min = 999999;
	int diff_max = 0;
	
	int best_diff = -1;
	
	unsigned iter = 0;
	bool any_valid = false;
	do {
		iter++;
		gamemap tmp;
		generate_one(tmp, par);
		
//if(!tmp.finished()){*this=tmp;return;}
		if (!tmp.finished()) abort();
tmp.reset();if(!tmp.solve())abort();
		if (par.allow_multi || !tmp.solve_another())
		{
			int diff = tmp.difficulty();
			
			if (diff > diff_max) diff_max = diff;
			if (diff < diff_min) diff_min = diff;
			
			float diff_scale = (diff-diff_min) / (float)(diff_max-diff_min);
			float best_diff_scale = (best_diff-diff_min) / (float)(diff_max-diff_min);
			if (diff_max==diff_min || fabs(diff_scale - par.difficulty) <= fabs(best_diff_scale - par.difficulty))
			{
				best_diff = diff;
				any_valid = true;
				*this = tmp;
			}
		}
	} while (!any_valid || (par.quality && iter<par.quality) || (!par.quality && !par.quality_stop(iter)));
//puts(serialize());
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
