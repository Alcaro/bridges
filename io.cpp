#include "game.h"
#include <valgrind/valgrind.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

static const KeySym keys[6] = { XK_Right, XK_Up, XK_Left, XK_Down, XK_Return, XK_BackSpace };
static int scans[6];

static void pressed_keys_init()
{
	Display* display = window_x11.display;
	
	int minkc;
	int maxkc;
	XDisplayKeycodes(display, &minkc, &maxkc);
	
	int sym_per_code;
	KeySym* sym = XGetKeyboardMapping(display, minkc, maxkc-minkc+1, &sym_per_code);
	
	//We want to process this backwards, so the unshifted state is the one that stays in the array.
	unsigned i = sym_per_code*(maxkc-minkc);
	while (i--)
	{
		for (int j=0;j<6;j++)
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
	for (int i=0;i<6;i++)
	{
		int scan = scans[i];
		if (state[scan>>3]&(1<<(scan&7)))
		{
			ret |= 1<<i;
		}
	}
	return ret;
}


int main(int argc, char** argv)
{
	argparse args;
	bool do_bench = false;
	args.add("perf", &do_bench);
	arlib_init(args, argv);
	
	if (do_bench)
	{
		srand(0);
		bool vg = RUNNING_ON_VALGRIND;
		uint64_t start = time_ms_ne();
		
		int nsolv = 0;
		int nunsolv = 0;
		int nmultisolv = 0;
		for (int side=3;side<9;side++)
		{
			for (int i=0;i<(vg ? 1000 : 10000);i++)
			{
				if (i%(vg ? 1 : 1000) == 0) { printf("%i %i     \r", side, i); fflush(stdout); }
				int minislands = side*2;
				int maxislands = side*side/2;
				int islands = minislands + rand()%(maxislands+1-minislands);
				string map = map_generate(side, side, islands);
				
				int result;
				int steps;
				map_solve(map, &result, 4, &steps);
				
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
	
	gl.Disable(GL_ALPHA_TEST);
	gl.Disable(GL_BLEND);
	gl.Disable(GL_DEPTH_TEST);
	gl.Disable(GL_POLYGON_SMOOTH);
	gl.Disable(GL_STENCIL_TEST);
	
	gl.Enable(GL_DITHER);
	gl.Enable(GL_TEXTURE_2D);
	
	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(0, 640, 0, 480, -1.0, 1.0);
	gl.Viewport(0, 0, 640, 480);
	
	gl.MatrixMode(GL_MODELVIEW);
	gl.LoadIdentity();
	
	GLuint tex;
	gl.GenTextures(1, &tex);
	gl.BindTexture(GL_TEXTURE_2D, tex);
	
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 512, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	
	pressed_keys_init();
	
	game* g = game::create();
	static uint32_t pixels[640*480]; // static to keep the stack frame small
	
	int lastkeys = 0;
	while (wnd->is_visible())
	{
//static int l=0;l++;if(l==120)break;
		int keys = pressed_keys();
		for (int i=0;i<6;i++)
		{
			if (keys & ~lastkeys & (1<<i))
			{
				g->key((game::key_t)i);
			}
		}
		lastkeys = keys;
		g->run(pixels);
		
		gl.ClearColor(1,0,1,0);
		gl.Clear(GL_COLOR_BUFFER_BIT);
		
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 480, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
		
		gl.Begin(GL_TRIANGLE_STRIP);
		gl.TexCoord2f(0,            0          ); gl.Vertex3i(0,   480, 0);
		gl.TexCoord2f(640.0/1024.0, 0          ); gl.Vertex3i(640, 480, 0);
		gl.TexCoord2f(0,            480.0/512.0); gl.Vertex3i(0,   0,   0);
		gl.TexCoord2f(640.0/1024.0, 480.0/512.0); gl.Vertex3i(640, 0,   0);
		gl.End();
		
		gl.swapBuffers();
		
		runloop::global()->step();
	}
	
	return 0;
}
