#include "game.h"
#include "resources.h"
#include <math.h>

//TODO:
//- end game screen
//- better selection icon for keyboard gameplay
//- better reef graphics
//- design some levels containing the new objects
//    is the generator good enough, or do I make a map editor?
//    make sure to introduce the new objects in a reasonable way
//- add hint system
//- restrict keyboard cursor position to the middle if the outermost parts have no possible bridges, I'll need it for mainlands
//- make solver consider maps unambiguous if all solutions have the same number of bridges between islands A and B,
//    to avoid handing out "this one is either 0 or 4" hints to people who know how generator works
//    for example, this map should be unambiguous
//      2 2
//      ^ ^
//    to do that, make solver always move same-island-pair bridges up/left, if possible
//      and if both edges are straight (to avoid issues with L-shaped islands where the bridge in the hole must go the other way)
//    alternatively, ban generator from creating maps where the situation shows up
//    alternatively, just leave it like this, generator will never be perfect

namespace {
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
	//if 'force', they'll appear nearby; if not, far off
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
		if (tileset != 2) // no birds in the snow region
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
	struct input prev_in;
	static const int k_click = 7; // used in in_push
	uint8_t in_press; // true for one frame only
	bool recent_change = false;
	
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
	
	int title_frame;
	
	int menu_focus;
	
	gamemap map;
	int map_id;
	bool game_menu;
	int game_menu_pos;
	int game_menu_focus;
	
#ifdef ARLIB_THREAD
	uint64_t gamegen_next[3] = { 0, 0, 0 };
	uint8_t gamegen_type;
	gamemap::generator* gamegen_core = NULL;
#endif
	
	int game_kb_x;
	int game_kb_y;
	uint8_t game_kb_visible; // 0 - invisible, anything else - recently used, reduced by 1 for every mouse click
	uint8_t game_kb_state; // 0 - keyboard inactive, focus box invisible; 1 - not focused, moving around; 2 - held down
	uint8_t game_kb_holdtimer; // if this hits 0 and the key is still held, move another step
	
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
		pop_tutor3d,
		
		pop_tutorrandom1,
		pop_tutorrandom2,
		
		//TODO: tutorials for the new objects
	};
	int popup_id;
	int popup_frame;
	bool popup_closed_with_kb;
	bool seen_random_tutorial = false;
	
	
	void init()
	{
		res_bg0b.init_clone(res.bg0, -1, 1);
		res_menuclose.init_clone(res.menuopen, 1, -1);
		res_titlebig.init_new(res.title.width*16, res.title.height*16, res.title.fmt);
		
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
			int off = (bgpos/6)%21;
			out.insert_tile(0, 0, 640, 480, res.bg1, off, 21-off);
		}
		if (tileset == 2)
		{
			out.insert_tile(0, 0, 640, 480, res.bg2);
		}
		
		bgpos++;
		//must be a multiple of
		//- 30 (bg0 animation)
		//- 6*21 (bg1 animation)
		//- 90 (castle flag animation)
		bgpos %= 630;
	}
	
	
	void to_title()
	{
		state = st_title;
		birdfg.reset(true);
		title_frame = 0;
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
		
		int titlescale = (16 - title_frame/2);
		if (titlescale < 4) titlescale = 4;
		
		if (title_frame <= 24) // to not waste cpu rerendering what's already there
		{
			res_titlebig.insert_scale_unsafe(0, 0, res.title, titlescale, titlescale);
		}
		out.insert_sub(319-(63*titlescale)/2, 144-(28*titlescale)/2, res_titlebig,
		               0, 0, res.title.width*titlescale, res.title.height*titlescale);
		
		if (title_frame > 48)
		{
			int fontscale = (8 - (title_frame-48)/2);
			if (fontscale < 2) fontscale = 2;
			res.smallfont.scale = fontscale;
			
			int textx = 320 - 25*fontscale;
			int texty = 349 - 9*fontscale/2;
			
			res.smallfont.color = 0x000000;
			out.insert_text(textx  , texty+1, res.smallfont, "Play Game");
			out.insert_text(textx+1, texty  , res.smallfont, "Play Game");
			
			res.smallfont.color = 0xFFFFFF;
			out.insert_text(textx  , texty  , res.smallfont, "Play Game");
		}
		
		if (title_frame > 60)
		{
			res.smallfont.scale = 1;
			out.insert_text(200, 195, res.smallfont, "keybol 2010");
			out.insert_text(380, 195, res.smallfont, "Alcaro 2018");
			
			if (in.mousex > 238 && in.mousex < 406 && in.mousey > 309 && in.mousey < 388)
			{
				// contrary to what this icon implies, the entire map is clickable
				// but it looks nice, so it stays
				out.insert(257, 344, res.menuchoice);
			}
		}
		else title_frame++;
		
		res.smallfont.scale = 2;
		
		if (in_press & 1<<k_confirm) to_menu(true);
		if (in_press & 1<<k_click) to_menu(false);
		
		birdfg.draw(out, res.bird, 0);
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
			else if (menu_focus >= 100) menu_focus = (menu_focus-100)*10 + 9;
			else if (menu_focus%5 > 0) menu_focus--;
		}
		if (in_press & 1<<k_right)
		{
			if (menu_focus == -1) menu_focus = 0;
			else if (menu_focus < 100 && menu_focus%5 < 4) menu_focus++;
			else if (menu_focus%10 == 9 && unlocked >= menu_focus) menu_focus = 100 + (menu_focus/10);
		}
		if (in_press & 1<<k_up)
		{
			if (menu_focus == -1) menu_focus = 0;
			else if (menu_focus >= 100) menu_focus = (menu_focus-100)*10 + 4;
			else if (menu_focus/5 > 0) menu_focus -= 5;
		}
		if (in_press & 1<<k_down)
		{
			if (menu_focus == -1) menu_focus = 0;
			else if (menu_focus >= 100) menu_focus = (menu_focus-100)*10 + 14;
			else if (menu_focus/5 < unlocked-1) menu_focus += 5;
		}
		if (menu_focus < 100 && menu_focus > unlocked-1) menu_focus = unlocked-1;
		
		if (in_press & 1<<k_confirm)
		{
			if (menu_focus != -1)
				to_game(menu_focus, true);
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
			"Firestorm - 7x7 Map (Very Hard)", // originally Hardcore, but I'm not sure how that compares to Hard
			"Constantine - 9x9 Map (Normal)",
			"Silverstone - 9x9 Map (Hard)",
		};
		
		auto draw_level = [this](int x, int y, int n){
			out.insert(x, y, res.levelbox);
			
			if (n < 100)
			{
				int d1 = (n+1)/10;
				int d2 = (n+1)%10;
				if (d1)
				{
					out.insert_text(x+6    + 2*(d1==1), y+8, res.smallfont, tostring(d1));
					out.insert_text(x+6+12 + 2*(d2==1), y+8, res.smallfont, tostring(d2));
				}
				else
				{
					out.insert_text(x+6+6  + 2*(d2==1), y+8, res.smallfont, tostring(d2));
				}
			}
			else
			{
				out.insert_text(x+6+6+1, y+8, res.smallfont, "?");
			}
			
			bool highlight = false;
			bool unlocked = (this->unlocked > n);
			if (n >= 100) unlocked = (this->unlocked >= (n-100)*5 + 11);
			if (unlocked && in.mousex >= x && in.mousex < x+35 && in.mousey >= y && in.mousey < y+35)
			{
				highlight = true;
				if (in_press & 1<<k_click)
					to_game(n, false);
			}
			if (menu_focus == n)
				highlight = true;
			if (highlight) // as a variable to make sure the box isn't drawn twice, that'd be ugly
				out.insert(x-10, y-10, res.levelboxactive);
		};
		for (int y=0;y<6;y++)
		{
			int oy = 38+y*75;
			
			bool has_randoms = (y&1);
			
			bool b = (y>=4); // the rock tile contains a repeating pattern, better follow its size
			out.insert_tile_with_border(150 - 30*has_randoms, oy-8, 340 + 60*has_randoms, 50,
			                            (y<2 ? res.fg0 : y<4 ? res.fg1menu : res.fg2),
			                            3+b, 37-b-b, 4+b, 36-b-b);
			
			for (int x=0;x<5+has_randoms;x++)
			{
				int ox = 182+60*x - 30*has_randoms;
				if (x==5) draw_level(ox, oy, 100 + y/2);
				else draw_level(ox, oy, y*5 + x);
			}
			
			if (unlocked <= (y+1)*5)
			{
				image outcrop;
				outcrop.init_ref(out);
				int hidefrom = 0;
				if (unlocked > y*5)
				{
					hidefrom = 170 + (unlocked - y*5)*60 - 30*has_randoms;
					outcrop.init_ref_sub(out, hidefrom,0, 640-hidefrom,480);
				}
				if (unlocked == (y+1)*5 && !has_randoms) hidefrom = 640;
				outcrop.insert_tile_with_border(150-hidefrom - 30*has_randoms, 30+y*75, 340 + 60*has_randoms, 50,
				                                (y<2 ? res.fg0mask : y<4 ? res.fg1mask : res.fg0mask),
				                                3+b, 37-b, 4+b, 36-b);
			}
			
			uint32_t textwidth = res.smallfont.measure(names[y]);
			out.insert_text(320-textwidth/2, oy-22, res.smallfont, names[y]);
		}
		
		//no birds on the menu
	}
	
	
	void to_game(int id, bool use_kb)
	{
		state = st_ingame;
		
		game_load(id, use_kb);
		game_menu = false;
		game_menu_pos = 480;
	}
	
	gamemap::genparams game_params(int id)
	{
		gamemap::genparams p = {};
		p.width = 7;
		p.height = 7;
		p.density = 0.4;
		p.quality = 20000;
		p.use_reef = true;
		p.use_large = true;
		//p.use_castle = true;
		//p.allow_multi = true;
		//p.allow_dense = true;
		
		if (id == 100)
		{
			p.density = 0.2;
			p.difficulty = 0.75; // this is somewhat high, but the low density makes all maps easy
		}
		if (id == 101)
		{
			p.difficulty = 1.0;
		}
		if (id == 102)
		{
			//larger maps have higher probabilitiy of having multiple solutions, so the remaining ones are usually easier
			p.difficulty = 1.0;
			p.width = 9;
			p.height = 9;
		}
		
		return p;
	}
	
	void game_load(int id, bool use_kb)
	{
		map_id = id;
		if (id < 30)
		{
			map.init(game_maps[id]);
			tileset = id/10;
		}
		else
		{
			gamemap::genparams p = game_params(id);
#ifdef ARLIB_THREAD
			if (gamegen_next[id-100])
			{
				gamemap::generator::unpack(p, gamegen_next[id-100], map);
				gamegen_next[id-100] = 0;
			}
			else // this can only happen if player enters a map, leaves, and reenters, within half a second (or savefile is corrupt)
			     // getting a generator delay is suboptimal, but unavoidable, and no human can click that fast
#endif
				map.generate(p);
			map.reset();
			
			tileset = id-100;
		}
		
		popup_id = pop_none;
		popup_frame = 0;
		if (id == 0) popup_id = pop_tutor1;
		if (id == 5) popup_id = pop_lv6p1;
		if (id == 10) popup_id = pop_lv11;
		if (id >= 100 && !seen_random_tutorial)
		{
			seen_random_tutorial = true;
			popup_id = pop_tutorrandom1;
		}
		
		birdfg.reset(false);
		
		game_kb_state = use_kb ? 1 : 0;
		game_kb_x = 0;
		game_kb_y = 0;
		
		recent_change = true;
	}
	
	void draw_island_fragment(int x, int y, bool right, bool up, bool left, bool down, bool downright)
	{
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
	
	void draw_island_fragment_border(int x, int y, bool right, bool up, bool left, bool down, uint32_t border)
	{
		if (out.fmt == ifmt_argb8888 || out.fmt == ifmt_bargb8888) border |= 0xFF000000;
		
		int ymul = out.stride/sizeof(uint32_t);
		
		uint32_t* pixels = out.pixels32 + y*ymul + x;
		if (!up)
		{
			int xs = (left ? -11 : 2);
			int xe = (right ? 51 : 38);
			for (int x=xs;x<xe;x++) pixels[ymul*2 + x] = border;
		}
		if (!down)
		{
			int xs = (left ? -11 : 2);
			int xe = (right ? 51 : 38);
			for (int x=xs;x<xe;x++) pixels[ymul*37 + x] = border;
		}
		if (!left)
		{
			int ys = (up ? -11 : 2);
			int ye = (down ? 51 : 38);
			for (int y=ys;y<ye;y++) pixels[y*ymul + 2] = border;
		}
		if (!right)
		{
			int ys = (up ? -11 : 2);
			int ye = (down ? 51 : 38);
			for (int y=ys;y<ye;y++) pixels[y*ymul + 37] = border;
		}
	}
	
	int ingame(bool can_skip)
	{
		// this conditional is a mess, but it reduces battery use
		if (can_skip && !recent_change &&
			in.keys == 0 && in_press == 0 &&
			// popups hide the mouse, which confuses prev_in
			((in.mousex == prev_in.mousex && in.mousey == prev_in.mousey) || popup_frame==16) &&
			tileset == 2 /* tileset 2 has no birds */ && !map.has_castles &&
			(popup_id == pop_none || popup_frame == 16) && !game_menu && game_menu_pos == 480)
		{
			recent_change = false;
			return -1;
		}
		recent_change = (in.keys || in_press);
		
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
		
		int sx = (320 - 24*map.width + 4); // +4 to center it
		int sy = (240 - 24*map.height + 4);
		
		//must draw islands before pop/bri/etc, to allow it to spread out
		for (int ty=0;ty<map.height;ty++)
		for (int tx=0;tx<map.width;tx++)
		{
			int x = sx + tx*48;
			int y = sy + ty*48;
			
			gamemap::island& here = map.map[ty][tx];
			
			if (here.population >= 0)
			{
				bool joinr = (here.bridges[0]==3);
				bool joinu = (here.bridges[1]==3);
				bool joinl = (here.bridges[2]==3);
				bool joind = (here.bridges[3]==3);
				
				bool large = (joinr || joinu || joinl || joind);
				if (large)
				{
					bool joindr = (joinr && joind && map.map[ty+1][tx+1].rootnode == here.rootnode);
					draw_island_fragment(x, y, joinr, joinu, joinl, joind, joindr);
				}
				else
				{
					out.insert(x, y, fg);
				}
			}
			if (here.population == -2)
			{
				out.insert(x, y, res.fg0mask);
			}
		}
		
		for (int ty=0;ty<map.height;ty++)
		for (int tx=0;tx<map.width;tx++)
		{
			int x = sx + tx*48;
			int y = sy + ty*48;
			
			gamemap::island& here = map.map[ty][tx];
			
			if (here.population < 0) continue;
			
			bool joinr = (here.bridges[0]==3);
			bool joinu = (here.bridges[1]==3);
			bool joinl = (here.bridges[2]==3);
			bool joind = (here.bridges[3]==3);
			
			bool isroot = (here.rootnode == ty*100+tx);
			
			if (isroot && here.population >= 80)
			{
				out.insert_sub(x+12, y+12, res.castle, 64*(here.population-80), 0, 64, 64);
				
				int flagx1 = 0; // these values won't be used, but gcc doesn't know that
				int flagx2 = 0;
				int flagy = 0;
				
				if (here.population == 80) { flagx1=16; flagx2=49; flagy=1; }
				if (here.population == 81) { flagx1=28; flagx2=28; flagy=3; }
				if (here.population == 82) { flagx1=20; flagx2=48; flagy=2; }
				if (here.population == 83) { flagx1=32; flagx2=32; flagy=2; }
				
				
				int waveframe = (x + y + bgpos/3)%30;
				
				bool upleft = (waveframe>=15);
				int cut = waveframe%15 * 2;
				if (cut > 16) cut = 16;
				//else cut = 0;
				
				out.insert_sub(x+12+flagx1,     y+12+flagy+  upleft, res.flags, 16*(here.population-80),     0, cut,    12);
				out.insert_sub(x+12+flagx1+cut, y+12+flagy+1-upleft, res.flags, 16*(here.population-80)+cut, 0, 16-cut, 12);
				out.insert_sub(x+12+flagx2,     y+12+flagy+  upleft, res.flags, 16*(here.population-80),     0, cut,    12);
				out.insert_sub(x+12+flagx2+cut, y+12+flagy+1-upleft, res.flags, 16*(here.population-80)+cut, 0, 16-cut, 12);
			}
			else if (isroot)
			{
				int plx; // population label x
				int ply;
				int blx;
				int bly;
				int px;
				int py;
				int bx;
				int by;
				
				if (joinr && joind && ((tx+ty)&1)) goto do_joind;
				
				if (joinr)
				{
					plx = 16;
					ply = 6;
					blx = 55;
					bly = 25;
					
					px = 16;
					py = 16;
					bx = 55;
					by = 6;
					goto align_popbri;
				}
				else if (joind)
				{
				do_joind: ;
					plx = 10;
					ply = 13;
					blx = 10;
					bly = 67;
					
					px = 10;
					py = 23;
					bx = 10;
					by = 48;
					
				align_popbri:
					if (here.population==1) px += 2;
					if (here.population<=9) px += 4;
					if (here.population==11) px += 2;
					if (here.totbridges==1) bx += 2;
					if (here.totbridges<=9) bx += 4;
					if (here.totbridges==11) bx += 2;
				}
				else
				{
					//else island is known small, so pop/bri>=10 won't happen
					
					plx = 4;
					ply = 4;
					blx = 18;
					bly = 26;
					
					px = 25 + (here.population==1)*3;
					py = 3;
					bx = 4 + (here.totbridges==1)*2;
					by = 18;
				}
				
				res.smallfont.scale = 1;
				res.smallfont.color = 0xFFFF00;
				out.insert_text(x+plx, y+ply, res.smallfont, "Pop.");
				res.smallfont.color = 0xFFFFFF;
				out.insert_text(x+blx, y+bly, res.smallfont, "Bri.");
				res.smallfont.scale = 2;
				
				res.smallfont.color = 0x000000;
				out.insert_text(x+px+1, y+py, res.smallfont, tostring(here.population));
				out.insert_text(x+px, y+py+1, res.smallfont, tostring(here.population));
				out.insert_text(x+bx+1, y+by, res.smallfont, tostring(here.totbridges));
				out.insert_text(x+bx, y+by+1, res.smallfont, tostring(here.totbridges));
				res.smallfont.color = 0xFFFF00;
				out.insert_text(x+px, y+py, res.smallfont, tostring(here.population));
				res.smallfont.color = 0xFFFFFF;
				out.insert_text(x+bx, y+by, res.smallfont, tostring(here.totbridges));
			}
			
			int totbridges = map.get(here.rootnode).totbridges;
			int population = map.get(here.rootnode).population;
			uint32_t border = (totbridges < population ? -1 : totbridges == population ? 0xFFFF00 : 0xFF0000);
			if (totbridges >= population)
			{
				draw_island_fragment_border(x, y, joinr, joinu, joinl, joind, border);
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
		
		int tx = (in.mousex - (sx-4) + 48)/48 - 1; // +48-1 to avoid -1/48=0 (-49/48=-2 is fine, all negative values are equal)
		int ty = (in.mousey - (sy-4) + 48)/48 - 1; // -4 to center it
		
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
			
			int msx = (in.mousex - (sx-4) + 48)%48;
			int msy = (in.mousey - (sy-4) + 48)%48;
			
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
					if (here.population < 0 && here.bridges[dir] == 0 && here.bridges[dir^1] != 0) continue;
					break;
				}
				
				if (shift == 0)
					goto no_phantom;
			}
			
			//if that bridge is up or left, move to the other island so I won't have to implement it multiple times
			//if it's ocean, force move
			if (here.population < 0)
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
		
		if (in_press&~(1<<k_cancel | 1<<k_click) && game_kb_state==0 && !game_menu)
		{
			game_kb_state = 1;
			in_press &= 1<<k_cancel;
		}
		
		
		if (in_press & 1<<k_click && game_kb_visible != 0)
		{
			game_kb_visible--;
			if (game_kb_visible == 0) game_kb_state = 0;
		}
		
		if (game_kb_state != 0 && !game_menu)
		{
			if ((in.keys == 1<<k_right || in.keys == 1<<k_up || in.keys == 1<<k_left || in.keys == 1<<k_down) &&
					(in_press&~(1<<k_click)) == 0)
			{
				if (--game_kb_holdtimer == 0)
				{
					game_kb_holdtimer = 3;
					in_press = in.keys;
				}
				game_kb_visible = 5;
			}
			else game_kb_holdtimer = 20;
			
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
				bool didanything = false;
				for (int dir=0;dir<4;dir++)
				{
					static_assert(k_right == 0);
					static_assert(k_up    == 1);
					static_assert(k_left  == 2);
					static_assert(k_down  == 3);
					if (!(in_press & 1<<dir)) continue;
					
					int x = game_kb_x;
					int y = game_kb_y;
					
					gamemap::island& here = map.map[y][x];
					
					int realdir = dir;
					if (here.population == -1)
					{
						if (realdir == 3) realdir = 1;
						if (realdir == 0) realdir = 2;
					}
					
					if (here.bridgelen[realdir] != -1)
					{
						if (realdir == 1)
						{
							y -= here.bridgelen[1];
							realdir = 3;
						}
						
						if (realdir == 2)
						{
							x -= here.bridgelen[2];
							realdir = 0;
						}
						
						map.toggle(x, y, realdir);
						didanything = true;
					}
					
					game_kb_state = 1;
				}
				
				if (didanything && map.finished())
				{
					popup_id = pop_welldone;
					popup_frame = 0;
				}
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
			//TODO: better image
			out.insert(sx + game_kb_x*48 + game_kb_state*10, sy + game_kb_y*48, res.kbfocus);
		}
		
		birdfg.draw(out, res.bird, tileset);
		
		out.insert(599, 459, tileset==2 ? res.menuopenborder : res.menuopen);
		
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
			if (map_id < 100) item(0, 15, "Level "+tostring(map_id+1));
			else item(0, 15, "Level ??");
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
//TODO
{
if(map.finished())map.solve_another();else map.solve();
}
			
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
					if (map_id >= 100)
					{
						game_load(map_id, popup_closed_with_kb);
						break;
					}
					if (map_id+1 >= unlocked) unlocked++;
					game_load(map_id+1, popup_closed_with_kb);
					break;
				case pop_tutor2:
					if (map_id < 5) popup_id = pop_tutor3a;
					else if (map_id < 10) popup_id = pop_tutor3b;
					else popup_id = pop_tutor3c;
					
					break;
					
				case pop_tutor3b:
				case pop_tutor3d:
					popup_id = pop_tutor4;
					break;
				case pop_tutor3c:
					if (map_id >= 100) popup_id = pop_tutor3d;
					else popup_id = pop_tutor4;
					break;
				
				case pop_tutor4:
				case pop_lv6p2:
				case pop_lv11:
				case pop_tutorrandom2:
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
					
					"\7              Click anywhere to continue...", // pop_welldone
					
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
					
					"\1For example, the Pop. 3 islands at the bottom " // pop_lv6b
					"left must have bridges to the right; therefore, "
					"the nearby Pop. 1 island cannot have a bridge "
					"upwards.",
					
					
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
					
					
					"\2The dark tiles block bridge building, and large " // pop_tutor3d
					"islands have more than four possible bridge "
					"directions.",
					
					
					"\1Welcome to the random levels! Each time you play " // pop_tutorrandom1
					"this, it's a new map. They're always solvable, but "
					"may be harder than the originals. Progress will "
					"not be saved.",
					
					"\1There are a few new objects here: Dark tiles " // pop_tutorrandom2
					"block bridge building, and large islands have "
					"more than four possible bridge directions. Good "
					"luck!",
					
					
					
					
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
					out.insert_text(158-1, 60-1, res.smallfont, "WELL DONE!", 6);
					out.insert_text(158+1, 60-1, res.smallfont, "WELL DONE!", 6);
					out.insert_text(158-1, 60+1, res.smallfont, "WELL DONE!", 6);
					out.insert_text(158+1, 60+1, res.smallfont, "WELL DONE!", 6);
					res.smallfont.color = 0xFF0000;
					out.insert_text(158,   60,   res.smallfont, "WELL DONE!", 6);
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
				
				if (popup_frame == 17)
					popup_frame--; // make sure it stays on the 'full popup visible' frame
			}
		}
		
		return 1;
	}
	
	void load(const savedat& dat)
	{
		unlocked = dat.unlocked;
		seen_random_tutorial = dat.seen_random_tutorial;
		
#ifdef ARLIB_THREAD
		gamegen_next[0] = dat.gen_seeds[0];
		gamegen_next[1] = dat.gen_seeds[1];
		gamegen_next[2] = dat.gen_seeds[2];
#endif
	}
	
	void save(savedat& dat)
	{
		dat.unlocked = unlocked;
		dat.seen_random_tutorial = seen_random_tutorial;
		
#ifdef ARLIB_THREAD
		dat.gen_seeds[0] = gamegen_next[0];
		dat.gen_seeds[1] = gamegen_next[1];
		dat.gen_seeds[2] = gamegen_next[2];
#else
		dat.gen_seeds[0] = 0;
		dat.gen_seeds[1] = 0;
		dat.gen_seeds[2] = 0;
#endif
	}
	
	void gamegen_step()
	{
#ifdef ARLIB_THREAD
		if (gamegen_core)
		{
			if (gamegen_core->done(NULL))
			{
				gamegen_next[gamegen_type] = gamegen_core->pack();
				delete gamegen_core;
				gamegen_core = NULL;
			}
			else return;
		}
		
		for (int i=0;i<3;i++)
		{
			if (!gamegen_next[i])
			{
				gamegen_core = new gamemap::generator(game_params(100+i));
				gamegen_type = i;
				return;
			}
		}
#endif
	}
	
	
	
	
	
	
	
	int run(const input& in, image& out, bool can_skip)
	{
		if (state == st_init)
			this->in = in;
		
		this->in_press = (in.keys & ~this->in.keys);
		if (in.mouseclick && !this->in.mouseclick)
			this->in_press |= 1<<k_click;
		this->prev_in = this->in;
		this->in = in;
		
		this->out.init_ref(out);
		
		if (state != st_init) // don't do too much the first frame
			gamegen_step();
		
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
				return ingame(can_skip);
		}
		return 1;
	}
};
}

game* game::create() { return new game_impl(); }
game* game::create(const savedat& dat) { game_impl* ret = new game_impl(); ret->load(dat); return ret; }
