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
				//int minislands = side*2;
				//int maxislands = side*side/2;
				//int islands = minislands + rand()%(maxislands+1-minislands);
				//string map = map_generate(side, side, islands);
				
				gamemap::genparams par = {};
				par.width = side;
				par.height = side;
				par.density = 0.4;
				par.allow_dense = true;
				par.use_reef = false;
				par.use_large = false;
				par.use_castle = false;
				par.allow_multi = true;
				par.difficulty = 0.0;
				par.quality = 1;
				
				gamemap map;
				map.generate(par);
				map.reset();
				int result = 0;
				if (map.solve())
				{
					result = 1;
					if (map.solve_another()) result = 2;
				}
				
				if (result == 0)
				{
					puts("ERROR");
					puts(map.serialize());
					exit(1);
				}
				if (result == -1) nunsolv++;
				if (result == 0) nunsolv++;
				if (result == 1) nsolv++;
				if (result == 2) nmultisolv++;
			}
		}
		uint64_t end = time_ms_ne();
		//expected (with side = 3..9, i = 0..9999, generate.cpp md5 94940171e4f1b91ccedf683c90a95930):
		//32161 solvable, 0 unsolvable, 37839 multiple solutions, took 4062 ms
		printf("%i solvable, %i unsolvable, %i multiple solutions, took %u ms\n",
		       nsolv, nunsolv, nmultisolv, (unsigned)(end-start));
		
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
			for (int i=0;i<100;i++)
			{
				g->run(in, out);
				frames++;
			}
			end = time_ms_ne();
		}
		
		printf("level select: %i frames in %u ms = %ffps\n", frames, (unsigned)(end-start), frames/(float)(end-start)*1000);
		
		g->run(in_press, out);
		
		start = time_ms_ne();
		end = start;
		while (end < start+5000)
		{
			for (int i=0;i<100;i++)
			{
				g->run(in, out);
				frames++;
			}
			end = time_ms_ne();
		}
		
		printf("ingame: %i frames in %u ms = %ffps\n", frames, (unsigned)(end-start), frames/(float)(end-start)*1000);
		
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
	
	//save support not implemented here, just hardcode something reasonable
	static const uint8_t saveraw[] = { 31,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	game::savedat save;
	static_assert(sizeof(save) == sizeof(saveraw));
	memcpy(&save, saveraw, sizeof(save));
	
	game* g = game::create(save);
	
	bool first = true;
	bool active = true;
	while (wnd->is_visible())
	{
		if (active)
		{
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
				in.lmousebutton = (flags & Button1Mask);
				in.rmousebutton = (flags & Button3Mask); // why is right 3, middle should be 3
			}
			else
			{
				in.mousex = -1;
				in.mousey = -1;
				in.lmousebutton = false;
				in.rmousebutton = false;
			}
			
#ifdef ARLIB_OPT
			uint64_t start = time_us_ne();
#endif
			int do_upload = g->run(in, out, !first);
			first = false;
#ifdef ARLIB_OPT
			uint64_t end = time_us_ne();
			if (end-start >= 2500)
				printf("rendered in %uus\n", (unsigned)(end-start));
#endif
			if (do_upload > 0) gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 480, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
			active = (do_upload >= 0);
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
		
		runloop::global()->step(!active);
		active = (wnd->is_active());
	}
	
	return 0;
}
