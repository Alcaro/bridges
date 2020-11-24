//@ header
#include "../font.h"
extern font smallfont;

//@ body
font smallfont;

//@ constructor
smallfont.init_from_image(smallfont_raw);
smallfont.scale = 2;
smallfont.height = 9;
smallfont.width[(uint8_t)' '] += 2; // extra cast to shut up Clang
