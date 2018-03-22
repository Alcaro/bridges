#include "game.h"
#include "resources.h"

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
		
		//1 if the next tile in that direction is an island, 2 if there's one ocean tile before the next island
		//-1 if a bridge there would run into the edge of the map
		//valid for both islands and ocean tiles
		//[0] is right, [1] is up, [2] left, [3] down
		int8_t bridgelen[4];
		
		//number of bridges exiting this tile in the given direction, 0-2
		//set for ocean tiles too; [0]=[2] and [1]=[3] for oceans
		uint8_t bridges[4];
	};
	
	island map[100][100];
	
	//these two are used only inside finished(), to check if the map is connected
	bool visited[100*100];
	int16_t towalk[100*100];
	
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
			
			char mapchar = inmap[y*(width+1) + x];
			if (mapchar == ' ') here.population = -1;
			else here.population = mapchar-'0';
			
			here.bridges[0] = 0;
			here.bridges[1] = 0;
			here.bridges[2] = 0;
			here.bridges[3] = 0;
			
			here.bridgelen[0] = -1;
			here.bridgelen[1] = -1;
			here.bridgelen[2] = -1;
			here.bridgelen[3] = -1;
			
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
			int newbridges = (here.bridges[0]+1)%3;
			for (int xx=0;xx<here.bridgelen[0];xx++)
			{
				map[y][x+xx  ].bridges[0] = newbridges;
				map[y][x+xx+1].bridges[2] = newbridges;
			}
		}
		else
		{
			island& here = map[y][x];
			int newbridges = (here.bridges[3]+1)%3;
			for (int yy=0;yy<here.bridgelen[3];yy++)
			{
				map[y+yy  ][x].bridges[3] = newbridges;
				map[y+yy+1][x].bridges[1] = newbridges;
			}
		}
	}
	
	//The map is finished if
	//(1) the sum of bridgelen[y][x][0..3] equals population[y][x] for all non-ocean tiles
	//(2) no bridges cross each other
	//(3) all tiles are reachable from each other by following bridges
	bool finished()
	{
		int ixstart;
		
		for (int y=0;y<height;y++)
		for (int x=0;x<width;x++)
		{
			island& here = map[y][x];
			
			if (here.population != -1) ixstart = y*100+x; // keep track of one arbitrary island
			
			//if not ocean and wrong number of bridges,
			if (here.population != -1 &&
			    here.bridges[0]+here.bridges[1]+here.bridges[2]+here.bridges[3] != here.population)
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

class game_impl : public game {
public:
	struct image bg0;
	struct image bg0b;
	struct image fg0;
	struct image fg0mask;
	struct image bg1;
	struct image fg1;
	struct image fg1alt;
	struct image fg1mask;
	struct image bg2;
	struct image fg2;
	struct image fg2mask;
	
	struct font smallfont;
	
	struct image brih;
	struct image briv;
	
	struct image levelbox;
	struct image levelboxgold;
	struct image levelboxactive;
	
	struct image islanddone;
	struct image arrowh;
	struct image arrowv;
	
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
	
	int menu_focus;
	
	gamemap map;
	int map_id;
	
	
	void init()
	{
		bg0.init_decode_png(res::water);
		bg0b.init_clone(bg0, -1, 1);
		fg0.init_decode_png(res::grassisland);
		fg0mask.init_decode_png(res::grassislandmask);
		bg1.init_decode_png(res::lava);
		fg1.init_decode_png(res::lavaisland);
		fg1alt.init_decode_png(res::lavaislandmenu);
		fg1mask.init_decode_png(res::lavaislandmask);
		bg2.init_decode_png(res::snow);
		fg2.init_decode_png(res::rockisland);
		fg2mask.init_decode_png(res::rockislandmask);
		
		brih.init_decode_png(res::bridgeh);
		briv.init_decode_png(res::bridgev);
		
		levelbox.init_decode_png(res::levelbox);
		levelboxgold.init_decode_png(res::levelboxgold);
		levelboxactive.init_decode_png(res::levelboxactive);
		
		islanddone.init_decode_png(res::islanddone);
		arrowh.init_decode_png(res::arrowh);
		arrowv.init_decode_png(res::arrowv);
		
		image font_tmp;
		font_tmp.init_decode_png(res::font);
		smallfont.init_from_image(font_tmp);
		smallfont.scale = 2;
		smallfont.height = 9;
	}
	
	void background()
	{
		if (tileset == 0)
		{
			out.insert_tile(0, 0, 640, 480, ((bgpos/15) & 1) ? bg0b : bg0);
		}
		if (tileset == 1)
		{
			out.insert_tile(0, 0, 640, 480, bg1);
		}
		if (tileset == 2)
		{
			out.insert_tile(0, 0, 640, 480, bg2);
		}
		
		bgpos++;
		bgpos %= 1200;
		
		//draw black birds in the fire region
		//no birds seem to exist in rock region
	}
	
	
	void to_title()
	{
		state = st_title;
to_game(0);
	}
	
	void title()
	{
		background();
		
		out.insert_tile_with_border(150, 65, 340, 150, fg0, 3, 37, 4, 36);
		out.insert(200, 280, fg0);
		out.insert(400, 280, fg0);
		out.insert(200, 380, fg0);
		out.insert(400, 380, fg0);
		out.insert_tile(238, 293, 164, 11, brih);
		out.insert_tile(238, 393, 164, 11, brih);
		out.insert_tile(206, 318, 11, 64, briv);
		out.insert_tile(224, 318, 11, 64, briv);
		out.insert_tile(406, 318, 11, 64, briv);
		out.insert_tile(424, 318, 11, 64, briv);
		
		cstring help = "King of Bridges is played on a square grid\n"
		               "with squares that represent islands. The\n"
		               "yellow number represents the population of\n"
		               "that island.";
		smallfont.color = 0x000000;
		out.insert_text_justified(10, 11, 300, 450, smallfont, help);
		out.insert_text_justified(11, 10, 300, 450, smallfont, help);
		smallfont.color = 0xFFFFFF;
		out.insert_text_justified(10, 10, 300, 450, smallfont, help);
		
		if (in_press & (1<<k_confirm | 1<<k_click)) to_menu();
	}
	
	
	void to_menu()
	{
		state = st_menu;
		menu_focus = -1;
	}
	
	void menu()
	{
		background();
		
int unlocked = 1;
		
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
			if (menu_focus != -1) to_game(menu_focus);
		}
		
		smallfont.color = 0xFFFFFF;
		for (int y=0;y<6;y++)
		{
			int b = (y>=4 ? 2 : 0); // the rock tile contains a repeating pattern, better follow its size
			out.insert_tile_with_border(150, 30+y*75, 340, 50,
			                            (y<2 ? fg0 : y<4 ? fg1alt : fg2),
			                            3+b, 37-b, 4+b, 36-b);
			
			for (int x=0;x<5;x++)
			{
				int ox = 182+60*x;
				int oy = 38+y*75;
				
				int n = y*5 + x + 1;
				int d1 = n/10;
				int d2 = n%10;
				
				out.insert(ox, oy, levelbox);
				if (x == 4 && y!=5) out.insert(ox, oy, levelboxgold);
				
				if (d1)
				{
					out.insert_text(ox+6    + 2*(d1==1), oy+8, smallfont, tostring(d1));
					out.insert_text(ox+6+12 + 2*(d2==1), oy+8, smallfont, tostring(d2));
				}
				else
				{
					out.insert_text(ox+6+6  + 2*(d2==1), oy+8, smallfont, tostring(d2));
				}
				
				bool highlight = false;
				if (unlocked >= y+1 && in.mousex >= ox && in.mousex < ox+35 && in.mousey >= oy && in.mousey < oy+35)
				{
					highlight = true;
					if (in_press & 1<<k_click)
					{
						to_game(y*5 + x);
					}
				}
				if (menu_focus == y*5 + x)
				{
					highlight = true;
				}
				if (highlight)
				{
					out.insert(ox-10, oy-10, levelboxactive);
				}
			}
			
			if (unlocked < y+1)
			{
				out.insert_tile_with_border(150, 30+y*75, 340, 50,
				                            (y<2 ? fg0mask : y<4 ? fg1mask : fg2mask),
				                            3+b, 37-b, 4+b, 36-b);
			}
		}
	}
	
	void to_game(int id)
	{
		state = st_ingame;
		map_id = id;
		map.init(game_maps[id]);
		tileset = id/10;
	}
	
	void ingame()
	{
if(in_press & 1<<k_cancel)to_menu();
		
		background();
		smallfont.color = 0x000000;
		out.insert_text(4, 3, smallfont, "Level "+tostring(map_id+1), 2);
		out.insert_text(5, 2, smallfont, "Level "+tostring(map_id+1), 2);
		smallfont.color = 0xFFFFFF;
		out.insert_text(4, 2, smallfont, "Level "+tostring(map_id+1), 2);
		
		image& fg = (tileset==0 ? fg0 : tileset==1 ? fg1 : fg2);
		
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
				int nbridge = here.bridges[0]+here.bridges[1]+here.bridges[2]+here.bridges[3];
				
				out.insert(x, y, fg);
				
				smallfont.scale = 1;
				smallfont.color = 0xFFFF00;
				out.insert_text(x+4, y+5, smallfont, "Pop.");
				smallfont.color = 0xFFFFFF;
				out.insert_text(x+18, y+26, smallfont, "Bri.");
				smallfont.scale = 2;
				
				int px = x+25 + (here.population==1)*3;
				int py = y+3;
				int bx = x+4 + (nbridge==1)*2;
				int by = y+18;
				
				smallfont.color = 0x000000;
				out.insert_text(px+1, py, smallfont, tostring(here.population));
				out.insert_text(px, py+1, smallfont, tostring(here.population));
				out.insert_text(bx+1, by, smallfont, tostring(nbridge));
				out.insert_text(bx, by+1, smallfont, tostring(nbridge));
				smallfont.color = 0xFFFF00;
				out.insert_text(px, py, smallfont, tostring(here.population));
				smallfont.color = 0xFFFFFF;
				out.insert_text(bx, by, smallfont, tostring(nbridge));
				
				if (here.population == nbridge)
				{
					out.insert(x, y, islanddone);
				}
				
				if (here.bridges[1] == 1)
				{
					out.insert_tile(x+14, y - here.bridgelen[1]*48 + 37, 11, here.bridgelen[1]*48-34, briv);
				}
				if (here.bridges[1] == 2)
				{
					out.insert_tile(x+6,  y - here.bridgelen[1]*48 + 37, 11, here.bridgelen[1]*48-34, briv);
					out.insert_tile(x+24, y - here.bridgelen[1]*48 + 37, 11, here.bridgelen[1]*48-34, briv);
				}
				if (here.bridges[2] == 1)
				{
					out.insert_tile(x - here.bridgelen[2]*48 + 37, y+13, here.bridgelen[2]*48-34, 11, brih);
				}
				if (here.bridges[2] == 2)
				{
					out.insert_tile(x - here.bridgelen[2]*48 + 37, y+6,  here.bridgelen[2]*48-34, 11, brih);
					out.insert_tile(x - here.bridgelen[2]*48 + 37, y+24, here.bridgelen[2]*48-34, 11, brih);
				}
			}
		}
		
		int tx = (in.mousex - sx + 48)/48 - 1; // +48-1 to avoid -1/48=0
		int ty = (in.mousey - sy + 48)/48 - 1;
		
		if (tx >= 0 && tx < map.width && ty >= 0 && ty < map.height)
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
				if (here.bridgelen[dir] != -1)
				{
					//clicking an ocean tile with a bridge cannot create a crossing bridge
					if (here.population == -1 && here.bridges[dir] == 0 && here.bridges[dir^1] != 0) continue;
					break;
				}
				
				if (shift == 0)
					goto no_phantom;
			}
			
			//if that bridge is up or left, move to the other island so I won't have to implement it multiple times
			//if it's ocean, force move, only islands have the bridge flags
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
				out.insert_tile_with_border(x+37, y+14, len*48-34, 12, arrowh, 6, 9, 0, 0);
			}
			else
			{
				out.insert_tile_with_border(x+13, y+37, 12, len*48-34, arrowv, 0, 0, 6, 9);
			}
			
			if (in_press & 1<<k_click)
			{
				map.toggle(tx, ty, dir);
				if (map.finished())
				{
					to_game((map_id+1) % 30);
				}
			}
		}
		no_phantom: ;
	}
	
	
	
	
	
	
	
	void run(const input& in, uint32_t* pixels, size_t stride)
	{
		this->in_press = (in.keys & ~this->in.keys);
		if (in.mouseclick && !this->in.mouseclick) this->in_press |= 1<<k_click;
		this->in = in;
		
		out.init_ptr((uint8_t*)pixels, 640, 480, stride, ifmt_0rgb8888);
		
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
