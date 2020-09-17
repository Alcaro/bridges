#include "game.h"

//#include <X11/Xlib.h>
//#include <X11/keysym.h>

int main(int argc, char** argv)
{
	argparse args;
#ifndef STDOUT_ERROR
	bool do_bench_solv = false;
	args.add("perf-solv", &do_bench_solv);
	bool do_bench_ui = false;
	args.add("perf-ui", &do_bench_ui);
#endif
	arlib_init(args, argv);
	
	static uint32_t pixels[640*480]; // static to keep the stack frame small
	image out; // initialized up here so perf-ui can use it too
	out.init_ptr(pixels, 640, 480, sizeof(uint32_t)*640, ifmt_xrgb8888); // xrgb is faster than 0rgb
	
#ifndef STDOUT_ERROR
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
			for (int i=0;i<(vg ? 1000 : 10000);i++) // ignore benchmark, we want iteration count constant between runs
			{
				if (i%(vg ? 10 : 100) == 0) { printf("%i %i     \r", side, i); fflush(stdout); }
				
				gamemap::genparams par = {};
				par.width = side;
				par.height = side;
				par.density = 0.4;
				par.dense = true;
				par.use_reef = false;
				par.use_large = false;
				par.use_castle = false;
				par.allow_ambiguous = true;
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
		//expected (with side = 3..9, i = 0..9999, generate.cpp md5 f32cac429a654b96cc327086541aac4b):
		//41535 solvable, 0 unsolvable, 28465 multiple solutions, took 7266 ms
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
		
		using (benchmark b)
		{
			while (b) g->run(in, out);
			printf("title screen: %u frames in %ums = %ffps\n", b.iterations, b.us/1000, b.per_second());
		}
		
		g->run(in_press, out);
		
		using (benchmark b)
		{
			while (b) g->run(in, out);
			printf("level select: %u frames in %ums = %ffps\n", b.iterations, b.us/1000, b.per_second());
		}
		
		g->run(in_press, out);
		
		using (benchmark b)
		{
			while (b) g->run(in, out);
			printf("ingame: %u frames in %ums = %ffps\n", b.iterations, b.us/1000, b.per_second());
		}
		
		return 0;
	}
#endif
	
	aropengl gl;
	autoptr<gameview> gv = gameview::create(gl, 640, 480, aropengl::t_ver_1_0/*|aropengl::t_direct3d_vsync*/, "bridges");
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
	
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1024, 512, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	
	//save support not implemented here, just hardcode something reasonable
	static const uint8_t save[] = { 31,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	game* g = game::create(save);
	game::input in = {};
	
	gv->keys_cb([&in](int scancode, gameview::key_t key, bool down) {
//printf("key %d,%d,%d\n",scancode,key,down);
		int bit;
		if(0);
		else if (key == gameview::K_RIGHT  || key == gameview::K_d)         bit = game::k_right;
		else if (key == gameview::K_UP     || key == gameview::K_w)         bit = game::k_up;
		else if (key == gameview::K_LEFT   || key == gameview::K_a)         bit = game::k_left;
		else if (key == gameview::K_DOWN   || key == gameview::K_s)         bit = game::k_down;
		else if (key == gameview::K_RETURN || key == gameview::K_SPACE)     bit = game::k_confirm;
		else if (key == gameview::K_ESCAPE || key == gameview::K_BACKSPACE) bit = game::k_cancel;
		else return;
		
		in.keys = (in.keys & ~(1<<bit)) | (down << bit);
	});
	
	gv->mouse_cb([&in](int x, int y, uint8_t buttons) {
//printf("mouse %d,%d,%d\n",x,y,buttons);
		in.mousex = x;
		in.mousey = y;
		in.lmousedown = (buttons&1);
		in.rmousedown = (buttons&2);
	});
	
	bool first = true;
	bool active = true;
	
	//time_t t=0;
	//int fps=0;
	
	while (gv->running())
	{
		//fps++;
		//if (t != time(NULL))
		//{
//printf("%dfps\n",fps);
			//fps = 0;
			//t = time(NULL);
		//}
		
		if (active)
		{
#if defined(ARLIB_OPT) && !defined(STDOUT_ERROR)
			timer t;
#endif
			int do_upload = g->run(in, out, !first);
			first = false;
#if defined(ARLIB_OPT) && !defined(STDOUT_ERROR)
			if (t.us() >= 2500)
				printf("rendered in %uus\n", (unsigned)t.us());
#endif
			if (do_upload > 0) gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 480, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
			active = (do_upload >= 0);
		}
		
		//redraw the texture, to avoid glitches if compositor discarded our pixels
		//no point reuploading, and no point checking why gv->tmp_step returned, just redraw
		gl.Clear(GL_COLOR_BUFFER_BIT); // does nothing, but docs say it makes things faster
		gl.Begin(GL_TRIANGLE_STRIP);
		gl.TexCoord2f(0,            0          ); gl.Vertex3i(0,   480, 0);
		gl.TexCoord2f(640.0/1024.0, 0          ); gl.Vertex3i(640, 480, 0);
		gl.TexCoord2f(0,            480.0/512.0); gl.Vertex3i(0,   0,   0);
		gl.TexCoord2f(640.0/1024.0, 480.0/512.0); gl.Vertex3i(640, 0,   0);
		gl.End();
		
		gl.swapBuffers();
		
		gv->tmp_step(!active);
		active = (gv->focused()); // keep them in this order, so the framestop implementation works
	}
	
	return 0;
}
