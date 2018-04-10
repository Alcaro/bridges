#include "game.h"
#include "resources.h"
#include <math.h>

//TODO:
//- end game screen
//- compressed UI, where islands are half size so maps can be up to 26x20
//- more objects:
//  - islands accepting an arbitrary number of bridges
//  - larger islands, 40x88px, can build 2 bridges from each of the 6 possible exits
//      cycle checker treats them as 2 separate islands with a mandatory bridge in between
//  - even bigger islands, 88x88
//  - maybe even huge non-rectangular islands, some of which extend off the edge of the map
//      if yes, they cannot bridge themselves
//  - castles; every island must be connected to a castle, but castles may not be connected to each other
//      actually, combine some of the above
//      castles must be on large (minimum 2x2) islands, and are arbitrary-populated (violating either rule would be cluttered)
//      multiple castles of the same color must be connected; multiple castles of different colors must be disjoint
//      all four castles have different designs (mostly grayscale, but colorful roofs), and 16x12 flags
//  - reef; can't build bridges across a reef
//- design some levels containing the new objects
//    is the randomizer good enough, or do I make a map editor?
//- make a new solver
//  - faster than the current one
//  - can return a single hint, rather than the full solution
//  - supports the new objects
//      how will it deal with castles? does it pick one castle of each color and pretend they're connected?
//        does that leave any actually-solvable map ambiguous?
//          for example (using RBYG for castles)
//            R2G
//             R
//          if the top two are fake-connected, the 2 could point right+down, which is illegal
//          similarly,
//            R22G
//             R
//          has the sole solution 'left 2 connects the reds, right 2 does both to green', but how does the solver prove that?
//          limiting to one castle per color would be silly
//          run the algorithm once for every combination of one castle of each color?
//        to bypass the four-connections-per-initial-tile limit,
//          the mandatory loop in a 2x2 island can be redirected to the next/previous castles
//- if possible, create difficulty estimator
//    at any point, how many bridges can be proven to exist or not exist? are they close to the edge?
//    does it involve the must-be-connected rule?
//    does it require guessing? how many tiles need to be considered to prove which is the case?
//    the solver can represent seven possible states for each possible bridge (plus unsolvable), humans can't; does this make it harder?
//- integrate game generator (requires solver)

//map structure for the new objects:
//- reef: #
//- large island: > ^ < v, ultimately pointing to a character defining the population (loops not allowed)
//- island with more than 9 pop: A=10, B=11, etc, up to Z=36 (above 36 would just be annoying)
//- castle: qwer (red, blue, yellow, green, respectively)
//    or tyui to render off-center to the right; asdf for below; ghjk for below-right
//    for ghjk, the three tiles below, right and below-right must be part of the same island
//    for qwer, all eight surrounding must be on the island
//    for asdf and tyui, five surrounding connected tiles are needed

//internal representation:
//- reef: population -2, bridgelen is -1 across the area
//- large islands:
//    every slot in the grid contains an int16, pointing to the root node of that island, where pop/bri is rendered
//    root nodes point to themselves; small islands (including oceans and reefs and small islands) have -1 there
//    the root node has a bridge counter variable, which is updated every time something connects to the island
//    bridgelen, if pointing to within the island, is -2; 'bridges' is 100 and not rendered
//- castles:
//    population is 80 plus color*1 plus moveright*4 plus movedown*8
//    index to one castle of each color is stored in the gamemap (-1 if none)
//    flood fill starts from them, one at the time; coloring an incorrect castle aborts the operation, correct is ignored
//    finding if all are connected is done via island counting

namespace {
class gamemap {
public:
	//known to be 100 or less
	//(UI supports 13x10, anything more goes out of bounds)
	int width;
	int height;
	
	int numislands;
	
	struct island {
		int8_t population; // -1 for ocean
		int8_t totbridges;
		int16_t rootnode; // -1 for ocean, y*100+x for most islands, something else for large islands
		
		//1 if the next tile in that direction is an island, 2 if there's one ocean tile before the next island
		//-1 if a bridge there would run into the edge of the map
		//valid for both islands and ocean tiles
		//[0] is right, [1] is up, [2] left, [3] down
		int8_t bridgelen[4];
		
		//number of bridges exiting this tile in the given direction, 0-2
		//set for ocean tiles too; [0]=[2] and [1]=[3] for oceans
		//if that island is part of the current one (rootnode equal), 3 bridges
		uint8_t bridges[4];
	};
	
	island map[100][100];
	
	//these two are used only inside finished(), to check if the map is connected
	bool visited[100*100];
	int16_t towalk[100*100];
	
	island& get(int key) { return map[key/100][key%100]; }
	
	void init(const char * inmap)
	{
		width = 0;
		while (inmap[width] != '\n') width++;
		height = 0;
		while (inmap[(width+1) * height] != '\0') height++;
		
		
		numislands = 0;
		
		for (int y=0;y<height;y++)
		for (int x=0;x<width;x++)
		{
			island& here = map[y][x];
			
			int rootpos = y*100+x;
			char mapchar = inmap[y*(width+1) + x];
			
			while (mapchar == '<' || mapchar == '>' || mapchar == 'v' || mapchar == '^')
			{
				if (mapchar == '>') rootpos += 1;
				if (mapchar == '^') rootpos -= 100;
				if (mapchar == '<') rootpos -= 1;
				if (mapchar == 'v') rootpos += 100;
				
				int y2 = rootpos/100;
				int x2 = rootpos%100;
				mapchar = inmap[y2*(width+1) + x2];
			}
			
			if (mapchar == ' ') here.population = -1;
			else if (mapchar >= '0' && mapchar <= '9') here.population = mapchar-'0';
			else here.population = mapchar-'A'+10;
			
			here.totbridges = 0;
			here.bridges[0] = 0;
			here.bridges[1] = 0;
			here.bridges[2] = 0;
			here.bridges[3] = 0;
			
			here.bridgelen[0] = -1;
			here.bridgelen[1] = -1;
			here.bridgelen[2] = -1;
			here.bridgelen[3] = -1;
			
			here.rootnode = rootpos;
			
			if (here.population != -1)
			{
				numislands++;
				
				for (int x2=x-1;x2>=0;x2--)
				{
					if (map[y][x2].population != -1)
					{
						here.bridgelen[2] = x-x2;
						map[y][x2].bridgelen[0] = x-x2;
						for (int x3=x2+1;x3<x;x3++)
						{
							map[y][x3].bridgelen[0] = x-x3;
							map[y][x3].bridgelen[2] = x3-x2;
						}
						
						if (map[y][x2].rootnode == here.rootnode && x-x2==1)
						{
							here.bridges[2] = 3;
							map[y][x2].bridges[0] = 3;
						}
						
						break;
					}
				}
				for (int y2=y-1;y2>=0;y2--)
				{
					if (map[y2][x].population != -1)
					{
						here.bridgelen[1] = y-y2;
						map[y2][x].bridgelen[3] = y-y2;
						for (int y3=y2+1;y3<y;y3++)
						{
							map[y3][x].bridgelen[1] = y3-y2;
							map[y3][x].bridgelen[3] = y-y3;
						}
						
						if (map[y2][x].rootnode == here.rootnode && y-y2==1)
						{
							here.bridges[1] = 3;
							map[y2][x].bridges[3] = 3;
						}
						
						break;
					}
				}
			}
		}
	}
	
	//only allows dir=0 or 3
	void toggle(int x, int y, int dir)
	{
		if (dir == 0)
		{
			island& here = map[y][x];
			if (here.bridges[0] == 3) return;
			
			int newbridges = (here.bridges[0]+1)%3;
			int diff = newbridges - here.bridges[0];
			for (int xx=0;xx<here.bridgelen[0];xx++)
			{
				map[y][x+xx  ].bridges[0] = newbridges;
				map[y][x+xx+1].bridges[2] = newbridges;
				get(map[y][x+xx  ].rootnode).totbridges += diff;
				get(map[y][x+xx+1].rootnode).totbridges += diff;
			}
		}
		else
		{
			island& here = map[y][x];
			if (here.bridges[3] == 3) return;
			
			int newbridges = (here.bridges[3]+1)%3;
			int diff = newbridges - here.bridges[3];
			for (int yy=0;yy<here.bridgelen[3];yy++)
			{
				map[y+yy  ][x].bridges[3] = newbridges;
				map[y+yy+1][x].bridges[1] = newbridges;
				get(map[y+yy  ][x].rootnode).totbridges += diff;
				get(map[y+yy+1][x].rootnode).totbridges += diff;
			}
		}
	}
	
	void reset()
	{
		for (int y=0;y<height;y++)
		for (int x=0;x<width;x++)
		{
			island& here = map[y][x];
			
			here.totbridges = 0;
			here.bridges[0] = 0;
			here.bridges[1] = 0;
			here.bridges[2] = 0;
			here.bridges[3] = 0;
		}
	}
	
	//The map is finished if
	//(1) the sum of bridgelen[y][x][0..3] equals population[y][x] for all non-ocean tiles
	//(2) no bridges cross each other
	//(3) all tiles are reachable from each other by following bridges
	bool finished()
	{
		int ixstart = 0;
		
		for (int y=0;y<height;y++)
		for (int x=0;x<width;x++)
		{
			island& here = map[y][x];
			
			if (here.population != -1) ixstart = y*100+x; // keep track of one arbitrary island
			
			//if not ocean and wrong number of bridges,
			if (here.population != -1 && here.rootnode == y*100+x && here.totbridges != here.population)
			{
				return false; // then map isn't solved
			}
			
			//if ocean and has both horizontal and vertical bridges,
			if (here.population == -1 &&
			    here.bridges[0] != 0 &&
			    here.bridges[3] != 0)
			{
				return false; // then map isn't solved
			}
		}
		
		//all islands are satisfied and there's no crossing - test if everything is connected
		int foundislands = 0;
		memset(visited, 0, sizeof(visited));
		
		int ntowalk = 0;
		towalk[ntowalk++] = ixstart;
		while (ntowalk)
		{
			int ix = towalk[--ntowalk];
			if (visited[ix]) continue;
			visited[ix] = true;
			
			island& here = map[ix/100][ix%100];
			foundislands++;
			
			if (here.bridges[0])
				towalk[ntowalk++] = ix + here.bridgelen[0];
			if (here.bridges[1])
				towalk[ntowalk++] = ix - here.bridgelen[1]*100;
			if (here.bridges[2])
				towalk[ntowalk++] = ix - here.bridgelen[2];
			if (here.bridges[3])
				towalk[ntowalk++] = ix + here.bridgelen[3]*100;
		}
		
		return (foundislands == numislands);
	}
};

class birds {
	static const int spd = 4;
	static const constexpr float flock_angle_rad = 155 * M_PI*2/360;
	static const int flock_dist = 12;
	
	float xmid;
	float ymid;
	
	float angle;
	int frame = 0; // if 0, birds are exactly at the middle; if <0, birds are moving towards the middle; if >0, moving away
	
public:
	//if birds are on-screen, moves them away
	//if 'force', they'll appear nearby; if not, 
	void reset(bool force)
	{
		if (frame < -800/spd) return; // far off = leave there, so they appear earlier
		
		//first, pick a center for the birds, anywhere in the screen that's at least 100px from the edge
		xmid = 100 + rand()%(640-200);
		ymid = 100 + rand()%(480-200);
		
		//then pick a direction, any direction
		angle = (float)rand()/RAND_MAX * M_PI*2;
		
		frame = -800/spd;
		if (!force)
		{
			frame -= 60*60 + rand()%(60*60);
		}
	}
	
	void draw(image& out, const image& birdsmap, int tileset)
	{
		if (tileset != 2) // no birds in the rock region
		{
			image bird;
			int animframe = frame/10 & 1;
			bird.init_ref_sub(birdsmap, animframe*8, tileset*6, 8, 6);
			
			int x = xmid + cos(angle)*spd*frame;
			int y = ymid + sin(angle)*spd*frame;
			
			for (int n=0;n<5;n++)
			{
				out.insert(x + cos(angle + flock_angle_rad)*n*flock_dist, y + sin(angle + flock_angle_rad)*n*flock_dist, bird);
				out.insert(x + cos(angle - flock_angle_rad)*n*flock_dist, y + sin(angle - flock_angle_rad)*n*flock_dist, bird);
			}
			
			frame++;
			
			//if outside the screen and distance increasing, reset them
			if (frame > 800/spd)
			{
				reset(false);
			}
		}
	}
};

class game_impl : public game {
public:
	resources res;
	image res_bg0b;
	image res_titlebig;
	image res_menuclose;
	
	struct input in;
	static const int k_click = 7; // used in in_push
	uint8_t in_press; // true for one frame only
	
	struct image out;
	
	
	enum state_t {
		st_init,
		st_title,
		st_menu,
		st_ingame
	};
	state_t state = st_init;
	
	int bgpos = 0;
	int tileset = 0;
	int unlocked = 1;
	
	birds birdfg;
	
	int menu_focus;
	
	gamemap map;
	int map_id;
	bool game_menu;
	int game_menu_pos;
	int game_menu_focus;
	
	int game_kb_x;
	int game_kb_y;
	uint8_t game_kb_state; // 0 - keyboard inactive, focus box invisible; 1 - not focused, moving around; 2 - held down
	
	enum {
		pop_none,
		pop_welldone,
		
		pop_tutor1,
		pop_tutor2,
		pop_tutor3a,
		pop_tutor4,
		pop_lv6p1,
		pop_lv6p2,
		pop_lv11,
		pop_tutor3b,
		pop_tutor3c,
		
		//TODO: tutorials for the new objects
	};
	int popup_id;
	int popup_frame;
	bool popup_closed_with_kb;
	
	
	void init()
	{
		res_bg0b.init_clone(res.bg0, -1, 1);
		res_menuclose.init_clone(res.menuopen, 1, -1);
		res_titlebig.init_clone(res.title, 4, 4);
		
		res.smallfont.scale = 2;
		res.smallfont.height = 9;
		res.smallfont.width[' '] += 2;
	}
	
	void background()
	{
		if (tileset == 0)
		{
			out.insert_tile(0, 0, 640, 480, ((bgpos/15) & 1) ? res_bg0b : res.bg0);
		}
		if (tileset == 1)
		{
			out.insert_tile(0, 0, 640, 480, res.bg1);
		}
		if (tileset == 2)
		{
			out.insert_tile(0, 0, 640, 480, res.bg2);
		}
		
		bgpos++;
		bgpos %= 1200;
	}
	
	
	void to_title()
	{
		state = st_title;
		birdfg.reset(true);
//to_game(0);
	}
	
	void title()
	{
		background();
		
		out.insert_tile_with_border(150, 65, 340, 150, res.fg0, 3, 37, 4, 36);
		out.insert(200, 285, res.fg0);
		out.insert(400, 285, res.fg0);
		out.insert(200, 375, res.fg0);
		out.insert(400, 375, res.fg0);
		out.insert_tile(238, 298, 164, 11, res.bridgeh);
		out.insert_tile(238, 388, 164, 11, res.bridgeh);
		out.insert_tile(206, 323, 11, 54, res.bridgev);
		out.insert_tile(224, 323, 11, 54, res.bridgev);
		out.insert_tile(406, 323, 11, 54, res.bridgev);
		out.insert_tile(424, 323, 11, 54, res.bridgev);
		
		out.insert(193, 88, res_titlebig);
		
		res.smallfont.color = 0x000000;
		out.insert_text_wrap(270, 341, 520, res.smallfont, "Play Game");
		out.insert_text_wrap(271, 340, 520, res.smallfont, "Play Game");
		
		res.smallfont.color = 0xFFFFFF;
		out.insert_text_wrap(270, 340, 520, res.smallfont, "Play Game");
		
		if (in_press & 1<<k_confirm) to_menu(true);
		if (in_press & 1<<k_click) to_menu(false);
		
		birdfg.draw(out, res.bird, 0);
		
/*
static const bool z[10][10] = {
{0,0,0,0,0,0,0,0,0,0},
{0,1,1,0,1,0,0,1,0,0},
{0,0,0,0,1,0,1,0,1,0},
{0,1,1,0,0,0,0,1,0,0},
{0,1,1,0,0,0,0,0,0,0},
{0,0,0,0,0,1,1,1,1,0},
{0,0,1,0,0,1,1,1,1,0},
{0,1,1,1,0,1,1,0,1,0},
{0,0,1,0,0,1,1,1,1,0},
{0,0,0,0,0,0,0,0,0,0},
};
background();
for (int y=0;y<10;y++)
for (int x=0;x<10;x++)
{
if (z[y][x])
{
draw_island_fragment(x*40, y*40, z[y][x+1], z[y-1][x], z[y][x-1], z[y+1][x]);
}
}
out.insert_tile_with_border(400, 20, 230, 440, res.fg0, 3, 37, 4, 36);
out.insert_tile(100, 20, 440, 440, res.levelboxgold);
//out.insert_tile(100, 20, 440, 440, res.fg0mask);
*/
	}
	
	
	void to_menu(bool use_keyboard)
	{
		tileset = 0;
		state = st_menu;
		menu_focus = (use_keyboard ? 0 : -1);
	}
	
	void menu()
	{
		background();
		
		if (in_press & 1<<k_left)
		{
			if (menu_focus == -1) menu_focus = 0;
			else if (menu_focus%5 > 0) menu_focus--;
		}
		if (in_press & 1<<k_right)
		{
			if (menu_focus == -1) menu_focus = 0;
			else if (menu_focus%5 < 4) menu_focus++;
		}
		if (in_press & 1<<k_up)
		{
			if (menu_focus == -1) menu_focus = 0;
			else if (menu_focus/5 > 0) menu_focus -= 5;
		}
		if (in_press & 1<<k_down)
		{
			if (menu_focus == -1) menu_focus = 0;
			else if (menu_focus/5 < unlocked-1) menu_focus += 5;
		}
		if (in_press & 1<<k_confirm)
		{
			if (menu_focus != -1) to_game(menu_focus, true);
		}
		if (in_press & 1<<k_cancel)
		{
			to_title();
		}
		
		res.smallfont.color = 0xFFFFFF;
		
		static const char * const names[6] = {
			"Kimera - 7x7 Map (Tutorial)",
			"Remnant - 7x7 Map (Normal)",
			"Alchemia - 7x7 Map (Hard)",
			"Firestorm - 7x7 Map (Hardcore)",
			"Constantine - 9x9 Map (Normal)",
			"Silverstone - 9x9 Map (Hard)",
		};
		
		for (int y=0;y<6;y++)
		{
			int oy = 38+y*75;
			
			bool b = (y>=4); // the rock tile contains a repeating pattern, better follow its size
			out.insert_tile_with_border(150, oy-8, 340, 50,
			                            (y<2 ? res.fg0 : y<4 ? res.fg1menu : res.fg2),
			                            3+b, 37-b-b, 4+b, 36-b-b);
			
			for (int x=0;x<5;x++)
			{
				int ox = 182+60*x;
				
				int n = y*5 + x + 1;
				int d1 = n/10;
				int d2 = n%10;
				
				out.insert(ox, oy, res.levelbox);
				if (x == 4 && y!=5) out.insert(ox, oy, res.levelboxgold);
				
				if (d1)
				{
					out.insert_text(ox+6    + 2*(d1==1), oy+8, res.smallfont, tostring(d1));
					out.insert_text(ox+6+12 + 2*(d2==1), oy+8, res.smallfont, tostring(d2));
				}
				else
				{
					out.insert_text(ox+6+6  + 2*(d2==1), oy+8, res.smallfont, tostring(d2));
				}
				
				bool highlight = false;
				if (unlocked >= y+1 && in.mousex >= ox && in.mousex < ox+35 && in.mousey >= oy && in.mousey < oy+35)
				{
					highlight = true;
					if (in_press & 1<<k_click)
					{
						to_game(y*5 + x, false);
					}
				}
				if (menu_focus == y*5 + x)
				{
					highlight = true;
				}
				if (highlight) // as a variable to make sure the box isn't drawn twice, that'd be ugly
				{
					out.insert(ox-10, oy-10, res.levelboxactive);
				}
			}
			
			if (unlocked < y+1)
			{
				out.insert_tile_with_border(150, 30+y*75, 340, 50,
				                            (y<2 ? res.fg0mask : y<4 ? res.fg1mask : res.fg0mask),
				                            3+b, 37-b, 4+b, 36-b);
			}
			
			uint32_t textwidth = res.smallfont.measure(names[y]);
			out.insert_text(320-textwidth/2, oy-22, res.smallfont, names[y]);
		}
		
		res.smallfont.scale = 1;
		uint32_t textwidth = res.smallfont.measure("Solve the golden puzzles to unlock the next set of puzzles!");
		out.insert_text(320-textwidth/2, 460, res.smallfont, "Solve the golden puzzles to unlock the next set of puzzles!");
		res.smallfont.scale = 2;
		
		//no birds on the menu
	}
	
	
	void to_game(int id, bool use_kb)
	{
		state = st_ingame;
		
		game_load(id, use_kb);
		game_menu = false;
		game_menu_pos = 480;
	}
	
	void game_load(int id, bool use_kb)
	{
		map_id = id;
		map.init(game_maps[id]);
		tileset = id/10;
		
		popup_id = pop_none;
		popup_frame = 0;
		if (id == 0) popup_id = pop_tutor1;
		if (id == 5) popup_id = pop_lv6p1;
		if (id == 10) popup_id = pop_lv11;
//popup_id = pop_welldone;
		
		birdfg.reset(false);
		
		game_kb_state = use_kb ? 1 : 0;
		game_kb_x = 0;
		game_kb_y = 0;
	}
	
	void draw_island_fragment(int x, int y, bool right, bool up, bool left, bool down, bool downright)
	{
//int tileset=1;
		image& im = (tileset==0 ? res.fg0 : tileset==1 ? res.fg1 : res.fg2);
		
		int bx = (tileset==2 ? 5 : 3); // border x
		int by = (tileset==2 ? 6 : 4);
		
		image im_tile;
		im_tile.init_ref_sub(im, bx, by, 40-bx*2, 40-by*2);
		
		if (left)
		{
			//top left
			out.insert_tile(x, y+(up?0:by), 20, 20-(up?0:by), im_tile, x, y+(up?0:by));
			if (!up)
				out.insert_sub(x-8, y, im, 6, 0, 28, by);
			
			//bottom left
			out.insert_tile(x, y+20, 20, 20-(down?-8:by), im_tile, x, y+20);
			if (!down)
				out.insert_sub(x-8, y+40-by, im, 6, 40-by, 28, by);
		}
		else
		{
			//top left
			out.insert_tile(x+bx, y+(up?0:by), 20-bx, 20-(up?0:by), im_tile, x+bx, y+(up?0:by));
			out.insert_sub(x, y-(up?8:0), im, 0, (up?6:0), bx, (up?28:20));
			if (!up)
				out.insert_sub(x+bx, y, im, bx, 0, 20-bx, by);
			
			//bottom left
			out.insert_tile(x+bx, y+20, 20-bx, 20-(down?-8:by), im_tile, x+bx, y+20);
			out.insert_sub(x, y+20, im, 0, (down?10:20), bx, 20-(down?-8:by));
			if (!down)
				out.insert_sub(x, y+40-by, im, 0, 40-by, 20, by);
		}
		
		if (right)
		{
			//top right
			out.insert_tile(x+20, y+(up?0:by), 28, 20-(up?0:by), im_tile, x+20, y+(up?0:by));
			if (!up)
				out.insert_sub(x+20, y, im, 6, 0, 28, by);
			
			//bottom right
			if (downright)
				out.insert_tile(x+20, y+20, 28, 20-(down?-8:by), im_tile, x+20, y+20);
			else
			{
				if (down) out.insert_tile(x+20, y+40, 20, 8, im_tile, x+20, y+40);
				out.insert_tile(x+20, y+20, 28, 20-(down?0:by), im_tile, x+20, y+20);
			}
			if (!down)
				out.insert_sub(x+20, y+40-by, im, 6, 40-by, 28, by);
		}
		else
		{
			//top right
			out.insert_tile(x+20, y+(up?0:by), 20-bx, 20-(up?-8:by), im_tile, x+20, y+(up?0:by));
			out.insert_sub(x+40-bx, y-(up?8:0), im, 40-bx, (up?6:0), bx, (up?28:20));
			if (!up)
				out.insert_sub(x+20, y, im, 20, 0, 20-bx, by);
			
			//bottom right
			out.insert_tile(x+20, y+20, 20-bx, 20-(down?-8:by), im_tile, x+20, y+20);
			out.insert_sub(x+40-bx, y+20, im, 40-bx, (down?10:20), bx, (down?28:20));
			if (!down)
				out.insert_sub(x+20, y+40-by, im, 20, 40-by, 20-bx, by);
		}
	}
	
	void ingame()
	{
		background();
		
		if (popup_id != pop_none)
		{
			if (in_press & (1<<k_confirm | 1<<k_click))
			{
				if (popup_frame < 18) popup_frame = 18;
				else popup_frame = 30;
				popup_closed_with_kb = (in_press & 1<<k_confirm);
			}
			in.mousex = -1;
			in.mousey = -1;
			in_press = 0;
		}
		
		image& fg = (tileset==0 ? res.fg0 : tileset==1 ? res.fg1 : res.fg2);
		
		int sx = (320 - 24*map.width);
		int sy = (240 - 24*map.height);
		
		for (int ty=0;ty<map.height;ty++)
		for (int tx=0;tx<map.width;tx++)
		{
			int x = sx + tx*48;
			int y = sy + ty*48;
			
			gamemap::island& here = map.map[ty][tx];
			
			if (here.population != -1)
			{
				bool joinr = (tx < map.width-1  && map.map[ty][tx+1].rootnode == here.rootnode);
				bool joinu = (ty > 0            && map.map[ty-1][tx].rootnode == here.rootnode);
				bool joinl = (tx > 0            && map.map[ty][tx-1].rootnode == here.rootnode);
				bool joind = (ty < map.height-1 && map.map[ty+1][tx].rootnode == here.rootnode);
				if (joinr || joinu || joinl || joind)
				{
					bool joindr = (joinr && joind && map.map[ty+1][tx+1].rootnode == here.rootnode);
					draw_island_fragment(x, y, joinr, joinu, joinl, joind, joindr);
				}
				else
				{
					out.insert(x, y, fg);
				}
				
				if (here.rootnode == ty*100+tx)
				{
					res.smallfont.scale = 1;
					res.smallfont.color = 0xFFFF00;
					out.insert_text(x+4, y+5, res.smallfont, "Pop.");
					res.smallfont.color = 0xFFFFFF;
					out.insert_text(x+18, y+26, res.smallfont, "Bri.");
					res.smallfont.scale = 2;
					
					int px = x+25 + (here.population==1)*3;
					int py = y+3;
					int bx = x+4 + (here.totbridges==1)*2;
					int by = y+18;
					
					res.smallfont.color = 0x000000;
					out.insert_text(px+1, py, res.smallfont, tostring(here.population));
					out.insert_text(px, py+1, res.smallfont, tostring(here.population));
					out.insert_text(bx+1, by, res.smallfont, tostring(here.totbridges));
					out.insert_text(bx, by+1, res.smallfont, tostring(here.totbridges));
					res.smallfont.color = 0xFFFF00;
					out.insert_text(px, py, res.smallfont, tostring(here.population));
					res.smallfont.color = 0xFFFFFF;
					out.insert_text(bx, by, res.smallfont, tostring(here.totbridges));
					
					if (here.totbridges == here.population)
						out.insert(x, y, res.islanddone);
					if (here.totbridges > here.population)
						out.insert(x, y, res.islandoverdone);
				}
				
				if (here.bridges[1] == 1)
				{
					out.insert_tile(x+14, y - here.bridgelen[1]*48 + 37, 11, here.bridgelen[1]*48-34, res.bridgev);
				}
				if (here.bridges[1] == 2)
				{
					out.insert_tile(x+6,  y - here.bridgelen[1]*48 + 37, 11, here.bridgelen[1]*48-34, res.bridgev);
					out.insert_tile(x+24, y - here.bridgelen[1]*48 + 37, 11, here.bridgelen[1]*48-34, res.bridgev);
				}
				if (here.bridges[2] == 1)
				{
					out.insert_tile(x - here.bridgelen[2]*48 + 37, y+13, here.bridgelen[2]*48-34, 11, res.bridgeh);
				}
				if (here.bridges[2] == 2)
				{
					out.insert_tile(x - here.bridgelen[2]*48 + 37, y+6,  here.bridgelen[2]*48-34, 11, res.bridgeh);
					out.insert_tile(x - here.bridgelen[2]*48 + 37, y+24, here.bridgelen[2]*48-34, 11, res.bridgeh);
				}
			}
		}
		
		int tx = (in.mousex - sx + 48)/48 - 1; // +48-1 to avoid -1/48=0 (-49/48=-2 is fine, all negative values are equal)
		int ty = (in.mousey - sy + 48)/48 - 1;
		
		//put this before bridge builder, to make sure menu wins if there's a possible bridge down there
		if (in_press & 1<<k_click && in.mousex >= 595 && in.mousex < 637 && in.mousey >= 455 && in.mousey < 477)
		{
			if (!game_menu)
			{
				game_menu = true;
				game_menu_focus = -1;
			}
			else game_menu = false;
			in_press &= ~(1<<k_click);
		}
		
		if (in.mousey < game_menu_pos && tx >= 0 && tx < map.width && ty >= 0 && ty < map.height)
		{
			//find which direction to build the bridge
			gamemap::island& here = map.map[ty][tx];
			
			int msx = (in.mousex - sx)%48;
			int msy = (in.mousey - sy)%48;
			
			//this cuts the island into 8 slices, numbered as follows:
			// 02
			//1  3
			//5  7
			// 46
			int slice = 0;
			if (msy >= 24)       slice ^= 4;
			if (msx >= 24)       slice ^= 2;
			if (msy >= msx)      slice ^= 1;
			if (msx >= (47-msy)) slice ^= 1;
			
			//then check if bridges are available in all four directions, in the order given by this table, using the first available
			static const uint8_t dirs[] = {
				1<<6 | 2<<4 | 0<<2 | 3<<0, // U L R D
				2<<6 | 1<<4 | 3<<2 | 0<<0, // L U D R
				1<<6 | 0<<4 | 2<<2 | 3<<0, // U R L D
				0<<6 | 1<<4 | 3<<2 | 2<<0, // R U D L
				3<<6 | 2<<4 | 0<<2 | 1<<0, // D L R U
				2<<6 | 3<<4 | 1<<2 | 0<<0, // L D U R
				3<<6 | 0<<4 | 2<<2 | 1<<0, // D R L U
				0<<6 | 3<<4 | 1<<2 | 2<<0, // R D U L
			};
			
			int dir;
			for (int shift=6;true;shift-=2)
			{
				dir = (dirs[slice]>>shift) & 3;
				if (here.bridgelen[dir] != -1 && here.bridges[dir] != 3)
				{
					//clicking an ocean tile with a bridge shouldn't create a crossing bridge
					if (here.population == -1 && here.bridges[dir] == 0 && here.bridges[dir^1] != 0) continue;
					break;
				}
				
				if (shift == 0)
					goto no_phantom;
			}
			
			//if that bridge is up or left, move to the other island so I won't have to implement it multiple times
			//if it's ocean, force move
			if (here.population == -1)
			{
				if (dir == 3) dir = 1;
				if (dir == 0) dir = 2;
			}
			
			if (dir == 1)
			{
				ty -= here.bridgelen[1];
				dir = 3;
			}
			
			if (dir == 2)
			{
				tx -= here.bridgelen[2];
				dir = 0;
			}
			
			int x = sx + tx*48;
			int y = sy + ty*48;
			
			//can't use 'here', ty/tx may have changed
			int len = map.map[ty][tx].bridgelen[dir];
			
			if (dir == 0)
			{
				out.insert_tile_with_border(x+37, y+14, len*48-34, 12, res.arrowh, 6, 9, 0, 0);
			}
			else
			{
				out.insert_tile_with_border(x+13, y+37, 12, len*48-34, res.arrowv, 0, 0, 6, 9);
			}
			
			if (in_press & 1<<k_click)
			{
				map.toggle(tx, ty, dir);
				if (map.finished())
				{
					popup_id = pop_welldone;
					popup_frame = 0;
				}
			}
		}
	no_phantom: ;
		
		if (in_press&~(1<<k_cancel | 1<<k_click) && game_kb_state==0)
		{
			game_kb_state = 1;
			game_kb_x = 0;
			game_kb_y = 0;
			
			in_press &= k_cancel;
		}
		if (game_kb_state != 0 && !game_menu)
		{
			if (in_press & 1<<k_confirm)
			{
				gamemap::island& here = map.map[game_kb_y][game_kb_x];
				if (here.bridgelen[0] != -1 || here.bridgelen[1] != -1 || here.bridgelen[2] != -1 || here.bridgelen[3] != -1)
				{
					game_kb_state = 3-game_kb_state; // toggle
				}
			}
			
			if (in.keys & 1<<k_confirm || game_kb_state == 2)
			{
				auto build = [this](int dir) {
					int x = game_kb_x;
					int y = game_kb_y;
					
					gamemap::island& here = map.map[y][x];
					
					if (here.population == -1)
					{
						if (dir == 3) dir = 1;
						if (dir == 0) dir = 2;
					}
					
					if (here.bridgelen[dir] != -1)
					{
						if (dir == 1)
						{
							y -= here.bridgelen[1];
							dir = 3;
						}
						
						if (dir == 2)
						{
							x -= here.bridgelen[2];
							dir = 0;
						}
						
						map.toggle(x, y, dir);
						if (map.finished())
						{
							popup_id = pop_welldone;
							popup_frame = 0;
						}
					}
					
					game_kb_state = 1;
				};
				
				if (in_press & 1<<k_right) build(0);
				if (in_press & 1<<k_up) build(1);
				if (in_press & 1<<k_left) build(2);
				if (in_press & 1<<k_down) build(3);
			}
			else
			{
				if ((in_press & 1<<k_right) && game_kb_x < map.width-1) game_kb_x++;
				if ((in_press & 1<<k_up   ) && game_kb_y > 0) game_kb_y--;
				if ((in_press & 1<<k_left ) && game_kb_x > 0) game_kb_x--;
				if ((in_press & 1<<k_down ) && game_kb_y < map.height-1) game_kb_y++;
			}
		}
		if (game_kb_state != 0)
		{
			out.insert(sx + game_kb_x*48 + game_kb_state*10, sy + game_kb_y*48, res.kbfocus);
		}
		
		birdfg.draw(out, res.bird, tileset);
		
		out.insert(600, 460, res.menuopen);
		
		if (in_press & 1<<k_cancel)
		{
			if (!game_menu)
			{
				game_menu = true;
				game_menu_focus = 2;
			}
			else game_menu = false;
		}
		if ( game_menu && game_menu_pos > 440) game_menu_pos -= 4;
		if (!game_menu && game_menu_pos < 480) game_menu_pos += 4;
		
		if (game_menu_pos < 480)
		{
			out.insert_tile_with_border(0, game_menu_pos, 640, 40, res.popupsm, 10,30, 0,40);
			
			if (in_press & 1<<k_left)
			{
				if (game_menu_focus == -1) game_menu_focus = 2;
				else if (game_menu_focus == 1) game_menu_focus = 4;
				else game_menu_focus -= 1;
			}
			if (in_press & 1<<k_right)
			{
				if (game_menu_focus == -1) game_menu_focus = 2;
				else if (game_menu_focus == 4) game_menu_focus = 1;
				else game_menu_focus += 1;
			}
			auto item = [this](int id, int x, cstring text) -> bool
			{
				int y = game_menu_pos + 10;
				
				res.smallfont.color = 0x000000;
				out.insert_text(x+1, y, res.smallfont, text);
				out.insert_text(x, y+1, res.smallfont, text);
				res.smallfont.color = 0xFFFFFF;
				int width = out.insert_text(x, y+1, res.smallfont, text);
				
				bool active = false;
				bool clicked = false;
				if (in.mousex >= x-7 && in.mousex <= x+width+7 && in.mousey >= y-3 && in.mousey < y+21)
				{
					active = true;
					if (in_press & 1<<k_click) clicked = true;
				}
				if (game_menu_focus == id && game_menu)
				{
					active = true;
					if (in_press & 1<<k_confirm) clicked = true;
				}
				if (id!=0 && active)
				{
					out.insert(x-11, y+6, res.menuchoice);
				}
				
				return clicked;
			};
			item(0, 15, "Level "+tostring(map_id+1));
			if (item(1, 120, "How to Play"))
			{
				popup_id = pop_tutor1;
				popup_frame = 0;
				game_menu = false;
			}
			if (item(2, 280, "Reset"))
			{
				map.reset();
				game_menu = false;
			}
			if (item(3, 375, "Level Select")) to_menu(in_press & 1<<k_confirm);
			if (item(4, 545, "Hint"))
game_menu_pos = 480; // TODO
			
			out.insert(600, game_menu_pos+18, res_menuclose);
			if (in_press & 1<<k_click && in.mousex >= 595 && in.mousex < 637 &&
			    in.mousey >= game_menu_pos+13 && in.mousey < game_menu_pos+35)
			{
				game_menu = false;
			}
		}
		
		if (popup_id != pop_none)
		{
			switch (popup_frame++ / 3)
			{
			case 10:
				switch (popup_id)
				{
				case pop_welldone:
					if (map_id == 29)
					{
tileset = 0;
to_title(); //TODO
						break;
					}
					if (map_id+1 >= unlocked*5) unlocked++;
					game_load(map_id+1, popup_closed_with_kb);
					break;
				case pop_tutor2:
					if (map_id < 5) popup_id = pop_tutor3a;
					else if (map_id < 10) popup_id = pop_tutor3b;
					else popup_id = pop_tutor3c;
					
					break;
					
				case pop_tutor3b:
				case pop_tutor3c:
					popup_id = pop_tutor4;
					break;
				case pop_tutor4:
				case pop_lv6p2:
				case pop_lv11:
					popup_id = pop_none;
					break;
				default:
					popup_id++;
					break;
				}
				popup_frame = 2;
				if (popup_id == 0) break;
				//else fallthrough
			case 0:
				out.insert_tile_with_border(290, 80, 60, 20, res.popup, 10,30, 0,110);
				break;
			case 1:
			case 9:
				out.insert_tile_with_border(240, 70, 160, 40, res.popup, 10,30, 0,110);
				break;
			case 2:
			case 8:
				out.insert_tile_with_border(190, 60, 260, 60, res.popup, 10,30, 0,110);
				break;
			case 3:
			case 7:
				out.insert_tile_with_border(140, 50, 360, 80, res.popup, 10,30, 0,110);
				break;
			case 4:
			case 6:
				out.insert_tile_with_border(90, 50, 460, 100, res.popup, 10,30, 0,110);
				break;
				
			case 5:
				out.insert_tile_with_border(40, 40, 560, 120, res.popup, 10,30, 0,120);
				
				static const char * const helptexts[] = {
					NULL,
					
					"\7            Click anywhere to continue...", // pop_welldone
					
					"\1Lord of Bridges is played on a square grid with " // pop_tutor1
					"squares that represent islands. The yellow "
					"number represents the population of that "
					"island.",
					
					"\1The white number represents the number of " // pop_tutor2
					"bridges connecting to the island. You can "
					"create up to two (2) bridges by clicking beside "
					"the islands.",
					
					"\0The first rule of the game is simple: Each island " // pop_tutor3a
					"must have as many bridges from it as the "
					"number in yellow in the island. There are two "
					"more rules, but this rule is enough to solve "
					"some simpler puzzles.",
					
					"\1Click on Arrow Icon (\3) on the bottom right to " // pop_tutor4
					"bring up menu items. Reset to remove all "
					"bridges, Hint if you're stuck and How to Play to "
					"see these instructions once again. Good luck!",
					
					
					"\2Good job! For this puzzle, you need to use the " // pop_lv6
					"second rule of the game: \1a bridge MUST NOT "
					"CROSS another bridge.\2",
					
					"\1For example, the Pop. 3 island at the top left " // pop_lv6b
					"must have a bridge to the right; therefore, the "
					"nearby Pop. 1 island cannot have a bridge "
					"downwards.",
					
					
					"\0Well done! This puzzle needs the third rule of " // pop_lv11
					"the game: \1All islands must be connected.\2 No "
					"isolated islands are allowed. The bottom left "
					"Pop. 1 islands cannot be connected to each "
					"other; their bridges must go rightwards.\2",
					
					
					"\2Each island must have as many bridges from it " // pop_tutor3b
					"as the number in yellow in the island. Also, \1a "
					"bridge MUST NOT CROSS another bridge.\2",
					
					
					"\0Each island must have as many bridges from it " // pop_tutor3c
					"as the number in yellow in the island. \1All islands "
					"must be connected.\2 No isolated islands are "
					"allowed. Also, \1a bridge MUST NOT CROSS another "
					"bridge.\2",
					
					
					
					
					//"\1King of Bridges is played on a square grid with " // pop_tutor1p1
					//"squares that represent islands. The yellow "
					//"number represents the population of that "
					//"island.",
					//
					//"\1The white number represents the number of " // pop_tutor1p2
					//"bridges connecting to the island. You can "
					//"create up to two (2) bridges by clicking beside "
					//"the islands.",
					//
					//"\0Each island must have as many bridges from it " // pop_tutor1p3
					//"as the number in yellow in the island. \1All islands "
					//"must be connected.\2 No isolated islands are "
					//"allowed. Also, \1a bridge MUST NOT CROSS another "
					//"bridge.\2",
					//
					//"\1Click on Arrow Icon (\3) on the bottom right to " // pop_tutor1p4
					//"bring up menu items. Reset to remove all "
					//"bridges, Hint if you're stuck and How to Play to "
					//"see these instructions once again. Good luck!",
					//
					//"\3Here is one starting technique to prep you. " // pop_tutor2p1
					//"Start on this lonely island with Pop. 1",
					//
					//"\5Now, the neighboring island must have 2 bridges to be connected on the other side.", // pop_tutor2bp1
					//
					//"\5The next steps to solve this area becomes easy as pie.", // pop_tutor2bp2
					//
					//"\1Corner islands with Pop. 4 must have 2 bridges " // pop_tutor3p1
					//"connected to each of its sides. The same goes "
					//"for islands with Pop. 6 and 3 neighbors and "
					//"islands with Pop. 8 and 4 neighbors.",
					//
					//"\1For islands with Pop. 5 and 3 neighbors, it is " // pop_tutor3p2
					//"not easy to tell where the 5 bridges will go. But "
					//"we're sure that at least 1 bridge will connect it "
					//"to each of the islands.",
					//
					//"\0Notice the 2 islands with Pop. 2 on the left. " // pop_tutor4p1
					//"Since the rules state that all islands must be "
					//"connected, we cannot therefore join these two "
					//"islands with 2 bridges as it will isolate them "
					//"from the rest.",
					//
					//"\1Try to place one (1) bridge on the opposite side " // pop_tutor4p2
					//"of these islands, since they must not have two "
					//"bridges in between them. Thereby, lowering our "
					//"choices by deduction method.",
					//
					//"\0With the knowledge you have, you can do a " // pop_tutor5p1
					//"combination of those techniques and solve any "
					//"puzzle easily. Of course, the fun part is the "
					//"discovery of new methods to solve the game and "
					//"sharing them! Good luck!",
				};
				
				if (popup_id == pop_welldone)
				{
					res.smallfont.scale = 6;
					res.smallfont.color = 0xFFFF00;
					out.insert_text(140-1, 60-1, res.smallfont, "WELL DONE!", 6);
					out.insert_text(140+1, 60-1, res.smallfont, "WELL DONE!", 6);
					out.insert_text(140-1, 60+1, res.smallfont, "WELL DONE!", 6);
					out.insert_text(140+1, 60+1, res.smallfont, "WELL DONE!", 6);
					res.smallfont.color = 0xFF0000;
					out.insert_text(140, 60, res.smallfont, "WELL DONE!", 6);
					res.smallfont.scale = 2;
				}
				
				int y = 48;
				y += helptexts[popup_id][0] * 11;
				
				res.smallfont.height = 11;
				res.smallfont.color = 0x000000;
				out.insert_text_wrap(60, y+1, 520, res.smallfont, helptexts[popup_id]+1);
				out.insert_text_wrap(61, y,   520, res.smallfont, helptexts[popup_id]+1);
				
				res.smallfont.width[3] = 17;
				res.smallfont.fallback = bind_lambda([this](image& out, const font& fnt, int32_t x, int32_t y, uint8_t ch)
					{
						if (ch == '\1') res.smallfont.color = 0xFFFF00;
						if (ch == '\2') res.smallfont.color = 0xFFFFFF;
						if (ch == '\3') out.insert(x, y+2, res.menuopen);
					});
				res.smallfont.color = 0xFFFFFF;
				out.insert_text_wrap(60, y, 520, res.smallfont, helptexts[popup_id]+1);
				res.smallfont.height = 9;
				res.smallfont.fallback = NULL;
				
				popup_frame--; // make sure it stays on the 'full popup visible' frame
			}
		}
	}
	
	void load(const savedat& dat)
	{
		unlocked = dat.unlocked;
	}
	
	void save(savedat& dat)
	{
		dat.unlocked = unlocked;
	}
	
	
	
	
	
	
	
	void run(const input& in, image& out)
	{
		this->in_press = (in.keys & ~this->in.keys);
		if (in.mouseclick && !this->in.mouseclick) this->in_press |= 1<<k_click;
		this->in = in;
		
		this->out.init_ref(out);
		
		switch (state)
		{
			case st_init:
				init();
				to_title();
				//fallthrough
			case st_title:
				title();
				break;
			case st_menu:
				menu();
				break;
			case st_ingame:
				ingame();
				break;
		}
	}
};
}

game* game::create() { return new game_impl(); }
game* game::create(const savedat& dat) { game_impl* ret = new game_impl(); ret->load(dat); return ret; }
