#pragma once
#include "arlib.h"

struct font {
	//one byte per row, maximum 8 rows
	//byte&1 is leftmost pixel, byte&0x80 is rightmost
	//1 is solid, 0 is transparent
	uint8_t characters[128][8];
	uint8_t width[128];
	uint8_t height;
	
	uint32_t color = 0x000000; // Top byte is ignored.
	uint8_t scale = 1;
	
	//Called if told to render characters 00-1F, except 0A (LF). Can draw whatever it wants, or change the font color.
	//If it draws, and the drawn item should have a width, width[ch] should be set as appropriate.
	//This callback may not set it, that'd confuse measure().
	function<void(image& img, const font& fnt, int32_t x, int32_t y, uint8_t ch)> fallback;
	
	//Expects spacesize in pixels, and returns the same. text may not contain linebreaks. Does not call the fallback.
	uint32_t measure(cstring text, float spacesize = 0);
	
	//The image must be pure black and white, containing 16x6 tiles of equal size.
	//Each tile must contain one left-aligned symbol, corresponding to its index in the ASCII table.
	//Each tile must be 8x8 or smaller. Maximum allowed image size is 128x48.
	void init_from_image(const image& img);
	
	//If xspace is nonzero, that many pixels (not multiplied by scale) are added after every space.
	//If align is true, a letter may only start at x + (integer * fnt.scale). If false, anywhere is fine.
	//Returns the width (in pixels) of the widest line.
	uint32_t render(image& img, int32_t x, int32_t y, cstring text, float xspace = 0, bool align = false) const;
	//If the line is at least width1 pixels, spaces are resized such that the line is approximately width2 pixels.
	//If the line is longer than width2, it overflows.
	void render_justified(image& img, int32_t x, int32_t y, uint32_t width1, uint32_t width2,
	                           cstring text, bool align = false) const;
	//Automatically inserts linebreaks to ensure everything stays within the given width.
	//Wrapped lines are justified, non-wrapped lines are left-aligned.
	void render_wrap(image& img, int32_t x, int32_t y, uint32_t width, cstring text) const;
};
