#include "game.h"
#include "arlib/deps/valgrind/valgrind.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>

static const KeySym keys[6*2] = { XK_Right,XK_d, XK_Up,XK_w, XK_Left,XK_a, XK_Down,XK_s, XK_Return,XK_space, XK_Escape,XK_BackSpace };
static int scans[6*2];

static void pressed_keys_init()
{
	int minkc;
	int maxkc;
	XDisplayKeycodes(window_x11.display, &minkc, &maxkc);
	
	int sym_per_code;
	KeySym* sym = XGetKeyboardMapping(window_x11.display, minkc, maxkc-minkc+1, &sym_per_code);
	
	for (int i=0;i<6*2;i++) scans[i] = -1;
	
	// process this backwards, so the unshifted state is the one that stays in the array
	unsigned i = sym_per_code*(maxkc-minkc);
	while (i--)
	{
		for (int j=0;j<6*2;j++)
		{
			if (sym[i] == keys[j]) scans[j] = minkc + i/sym_per_code;
		}
	}
	
	XFree(sym);
}

static int pressed_keys()
{
	uint8_t state[32];
	if (!XQueryKeymap(window_x11.display, (char*)state)) return 0;
	
	int ret = 0;
	for (int i=0;i<6*2;i++)
	{
		int scan = scans[i];
		if (scan >= 0 && state[scan>>3]&(1<<(scan&7)))
		{
			ret |= 1<<(i/2);
		}
	}
	return ret;
}


int main(int argc, char** argv)
{
	argparse args;
	bool do_bench_solv = false;
	args.add("perf-solv", &do_bench_solv);
	bool do_bench_ui = false;
	args.add("perf-ui", &do_bench_ui);
	arlib_init(args, argv);
	
	static uint32_t pixels[640*480]; // static to keep the stack frame small
	image out; // initialized up here so perf-ui can use it too
	out.init_ptr(pixels, 640, 480, sizeof(uint32_t)*640, ifmt_xrgb8888); // xrgb is faster than 0rgb
	
	if (do_bench_solv && do_bench_ui)
	{
		puts("can't benchmark both solver and UI");
		return 1;
	}
	if (do_bench_solv)
	{

//while (true)
//{
//again: ;
//
////is-island, do-down, do-right
////top left forced to be 2s, to not trivialize the others
//uint8_t bridge[7][9][3] = {
//	{ {1,2,2},{0    },{0    },{0    },{0    },{0    },{0    },{0    },{1,1,0} },
//	{ {1,1,1},{0    },{1,0,1},{0    },{1,1,1},{0    },{1,0,0},{0    },{0    } },
//	{ {0    },{1,1,1},{0    },{1,0,0},{0    },{1,0,1},{0    },{1,1,0},{0    } },
//	{ {1,1,0},{0    },{1,0,1},{0    },{1,1,1},{0    },{1,0,0},{0    },{0    } },
//	{ {0    },{1,0,1},{0    },{1,1,0},{0    },{1,0,1},{0    },{1,1,0},{0    } },
//	{ {1,0,1},{0    },{1,0,0},{0    },{1,0,1},{0    },{1,0,0},{0    },{0    } },
//	{ {0    },{1,0,1},{0    },{1,0,1},{0    },{1,0,1},{0    },{1,0,1},{1,0,0} },
//};
//
//for (int y=0;y<7;y++)
//for (int x=0;x<9;x++)
//{
//	if (bridge[y][x][1] && rand()%100 < 50) bridge[y][x][1] = 2;
//	if (bridge[y][x][2] && rand()%100 < 50) bridge[y][x][2] = 2;
//}
//
//string out = "";
//for (int y=0;y<7;y++)
//{
//for (int x=0;x<9;x++)
//{
//	int bri = 0;
//	bri += bridge[y][x][1];
//	bri += bridge[y][x][2];
//	for (int yy=y-1;yy>=0;yy--)
//	{
//		if (bridge[yy][x][0]) { bri += bridge[yy][x][1]; break; }
//	}
//	for (int xx=x-1;xx>=0;xx--)
//	{
//		if (bridge[y][xx][0]) { bri += bridge[y][xx][2]; break; }
//	}
//	if (bridge[y][x][0]) out += (char)('0'+bri);
//	else out += ' ';
//	
//	if (y==3 && x==4 && bri!=4) goto again;
//}
//out+="\n";
//}
//
//puts(out);
//
//int result;
//int steps;
//map_solve(out, &result, 3, &steps);
//
//if (result == 1 && steps > 0) exit(0);
//
////"4-------4\n" // test map
////"6-4-6-2 |\n"
////"|4-2|2-4|\n"
////"4|2-8-2||\n"
////"|4-4|2-6|\n"
////"4-2|4-2||\n"
////" 2-6-4-64\n"
//
//
//}



		if (false)
		{
		srand(0);
		bool vg = RUNNING_ON_VALGRIND;
		uint64_t start = time_ms_ne();
		
		int nsolv = 0;
		int nunsolv = 0;
		int nmultisolv = 0;
		for (int side=3;side<10;side++)
		{
			for (int i=0;i<(vg ? 1000 : 10000);i++)
			{
				if (i%(vg ? 1 : 1) == 0) { printf("%i %i     \r", side, i); fflush(stdout); }
				int minislands = side*2;
				int maxislands = side*side/2;
				int islands = minislands + rand()%(maxislands+1-minislands);
				string map = map_generate(side, side, islands);
//if(side==5&&i==2017)puts("##########\n"+map+"##########");
				
				int result;
				int steps;
				map_solve(map, &result, 3, &steps);
				
				//gamemap map2;
				//map2.init(map);
				//int result2 = 0;
				//if (solver_solve(map2))
				//{
				//	result2 = 1;
				//	if (solver_solve_another(map2)) result2 = 2;
				//}
				//
				//if (result != result2 && (result!=-1 || result2!=2))
				//{
				//	printf("%i:%i: solv1=%i solv2=%i\n###\n%s###\n", side, i, result, result2, (const char*)map);
				//}
				
				if (result == 1 && steps >= 3)
				//if (steps >= 5)
				{
					puts("MULTISTEP "+tostring(steps)+" "+tostring(result));
					puts(map);
				}
				if (result == 0)
				{
					puts("ERROR");
					puts(map);
					exit(1);
				}
				if (result == -1) nunsolv++;
				if (result == 0) nunsolv++;
				if (result == 1) nsolv++;
				if (result == 2) nmultisolv++;
			}
		}
		uint64_t end = time_ms_ne();
		printf("%i solvable, %i unsolvable, %i multiple solutions, took %u ms\n",
		       nsolv, nunsolv, nmultisolv, (unsigned)(end-start));
		}

		if (true)
		{
		srand(0);
		bool vg = RUNNING_ON_VALGRIND;
		uint64_t start = time_ms_ne();
		
		int nsolv = 0;
		int nunsolv = 0;
		int nmultisolv = 0;
		for (int side=3;side<10;side++)
		{
			for (int i=0;i<(vg ? 1000 : 10000);i++)
			{
				if (i%(vg ? 10 : 100) == 0) { printf("%i %i     \r", side, i); fflush(stdout); }
				int minislands = side*2;
				int maxislands = side*side/2;
				int islands = minislands + rand()%(maxislands+1-minislands);
				string map = map_generate(side, side, islands);
//if(side==15&&i>=6700)puts("##########\n"+map+"\n##########");
				
				gamemap map2;
				map2.init(map);
				int result = 0;
				if (solver_solve(map2))
				{
					result = 1;
					if (solver_solve_another(map2)) result = 2;
				}
				
				//int result;
				int steps = 1;
				//map_solve(map, &result, 3, &steps);
				
				if (result == 1 && steps >= 3)
				//if (steps >= 5)
				{
					puts("MULTISTEP "+tostring(steps)+" "+tostring(result));
					puts(map);
				}
				if (result == 0)
				{
					puts("ERROR");
					puts(map);
					exit(1);
				}
				if (result == -1) nunsolv++;
				if (result == 0) nunsolv++;
				if (result == 1) nsolv++;
				if (result == 2) nmultisolv++;
			}
		}
		uint64_t end = time_ms_ne();
		printf("%i solvable, %i unsolvable, %i multiple solutions, took %u ms\n",
		       nsolv, nunsolv, nmultisolv, (unsigned)(end-start));
		}
		
		return 0;
	}
	if (do_bench_ui)
	{
		game* g = game::create();
		game::input in = {};
		game::input in_press = {};
		in_press.keys = 1 << game::k_confirm;
		
		int frames = 0;
		
		uint64_t start = time_ms_ne();
		
		uint64_t end = start;
		while (end < start+5000)
		{
			for (int i=0;i<100;i++) // only checking timer every 100 frames boosts framerate from 1220 to 1418
			{
				g->run(in, out);
				frames++;
			}
			end = time_ms_ne();
		}
		
		printf("title screen: %i frames in %u ms = %ffps\n", frames, (unsigned)(end-start), frames/(float)(end-start)*1000);
		
		g->run(in_press, out);
		
		start = time_ms_ne();
		end = start;
		while (end < start+5000)
		{
			for (int i=0;i<100;i++) // only checking timer every 100 frames boosts framerate from 1220 to 1418
			{
				g->run(in, out);
				frames++;
			}
			end = time_ms_ne();
		}
		
		printf("level select: %i frames in %u ms = %ffps\n", frames, (unsigned)(end-start), frames/(float)(end-start)*1000);
		
		return 0;
	}
	
	widget_viewport* view;
	window* wnd = window_create(
		view = widget_create_viewport(640, 480)
		);
	wnd->show();
	
	aropengl gl;
	gl.create(view, aropengl::t_ver_1_0);
	gl.swapInterval(1);
	
	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(0, 640, 0, 480, -1.0, 1.0);
	gl.Viewport(0, 0, 640, 480);
	
	gl.MatrixMode(GL_MODELVIEW);
	gl.LoadIdentity();
	
	gl.Enable(GL_TEXTURE_2D);
	GLuint tex;
	gl.GenTextures(1, &tex);
	gl.BindTexture(GL_TEXTURE_2D, tex);
	
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 512, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	
	pressed_keys_init();
	
	//save support not implemented here, just hardcode a save file
	static const uint8_t saveraw[] = { 6 };
	game::savedat save;
	static_assert(sizeof(save) && sizeof(saveraw));
	memcpy(&save, saveraw, sizeof(save));
	
	game* g = game::create(save);
	
	goto enter; // force render the first frame, to avoid drawing an uninitialized texture
	while (wnd->is_visible())
	{
		if (wnd->is_active())
		{
	enter: ;
			game::input in;
			in.keys = pressed_keys();
			
			Window ignorew;
			int ignorei;
			unsigned flags;
			XQueryPointer(window_x11.display, view->get_parent(),
			              &ignorew, &ignorew, &ignorei, &ignorei,
			              &in.mousex, &in.mousey, &flags);
			if (in.mousex >= 0 && in.mousey >= 0 && in.mousex < 640 && in.mousey < 480)
			{
				in.mouseclick = (flags & Button1Mask);
			}
			else
			{
				in.mousex = -1;
				in.mousey = -1;
				in.mouseclick = false;
			}
			
			g->run(in, out);
			gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 480, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
		}
		
		//redraw the texture, to avoid glitches if we're sent an Expose event
		//no point reuploading, and no point caring about whether the event was Expose or not, just redraw
		gl.Clear(GL_COLOR_BUFFER_BIT); // does nothing, but likely makes things faster
		gl.Begin(GL_TRIANGLE_STRIP);
		gl.TexCoord2f(0,            0          ); gl.Vertex3i(0,   480, 0);
		gl.TexCoord2f(640.0/1024.0, 0          ); gl.Vertex3i(640, 480, 0);
		gl.TexCoord2f(0,            480.0/512.0); gl.Vertex3i(0,   0,   0);
		gl.TexCoord2f(640.0/1024.0, 480.0/512.0); gl.Vertex3i(640, 0,   0);
		gl.End();
		
		gl.swapBuffers();
		
		runloop::global()->step(!wnd->is_active());
	}
	
	return 0;
}
