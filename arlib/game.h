#ifdef ARGAME
#include "global.h"
#include "string.h"
#include "opengl/aropengl.h"

// You can only have one gameview per process.
class gameview : nocopy {
#ifndef _WIN32
static uintptr_t root_parent();
static gameview* create_finish(uintptr_t window, uint32_t width, uint32_t height, cstring windowtitle);
#else
static gameview* create(uint32_t width, uint32_t height, cstring windowtitle, uintptr_t* parent, uintptr_t** child);
#endif

public:
#ifndef _WIN32
template<typename Taropengl>
static gameview* create(Taropengl& gl, uint32_t width, uint32_t height, uint32_t glflags, cstring windowtitle)
{
	uintptr_t window;
	if (!gl.create(width, height, root_parent(), &window, glflags)) return NULL;
	return create_finish(window, width, height, windowtitle);
}
#else
template<typename Taropengl>
static gameview* create(Taropengl& gl, uint32_t width, uint32_t height, uint32_t glflags, cstring windowtitle)
{
	// GL only works on child windows on windows
	uintptr_t parent;
	uintptr_t* child;
	gameview* ret = create(width, height, windowtitle, &parent, &child);
	if (!gl.create(width, height, parent, child, glflags)) { delete ret; return NULL; }
	return ret;
}
#endif

virtual void exit_cb(function<void()> cb) = 0; // Called when the window is closed. Defaults to stop().
virtual bool running() = 0; // Returns false if the window was closed.
virtual void stop() = 0; // Makes running() return false.

virtual bool focused() = 0; // Returns false if something else is focused.

virtual ~gameview() {}


virtual void tmp_step(bool wait) = 0;


// compatible with libretro.h retro_key
enum key_t {
	K_UNKNOWN        = 0,   K_FIRST          = 0,
	K_BACKSPACE      = 8,   K_TAB            = 9,   K_CLEAR          = 12,  K_RETURN         = 13,  K_PAUSE          = 19,
	K_ESCAPE         = 27,  K_SPACE          = 32,  K_EXCLAIM        = 33,  K_QUOTEDBL       = 34,  K_HASH           = 35,
	K_DOLLAR         = 36,  K_AMPERSAND      = 38,  K_QUOTE          = 39,  K_LEFTPAREN      = 40,  K_RIGHTPAREN     = 41,
	K_ASTERISK       = 42,  K_PLUS           = 43,  K_COMMA          = 44,  K_MINUS          = 45,  K_PERIOD         = 46,
	K_SLASH          = 47,  K_0              = 48,  K_1              = 49,  K_2              = 50,  K_3              = 51,
	K_4              = 52,  K_5              = 53,  K_6              = 54,  K_7              = 55,  K_8              = 56,
	K_9              = 57,  K_COLON          = 58,  K_SEMICOLON      = 59,  K_LESS           = 60,  K_EQUALS         = 61,
	K_GREATER        = 62,  K_QUESTION       = 63,  K_AT             = 64,  K_LEFTBRACKET    = 91,  K_BACKSLASH      = 92,
	K_RIGHTBRACKET   = 93,  K_CARET          = 94,  K_UNDERSCORE     = 95,  K_BACKQUOTE      = 96,  K_a              = 97,
	K_b              = 98,  K_c              = 99,  K_d              = 100, K_e              = 101, K_f              = 102,
	K_g              = 103, K_h              = 104, K_i              = 105, K_j              = 106, K_k              = 107,
	K_l              = 108, K_m              = 109, K_n              = 110, K_o              = 111, K_p              = 112,
	K_q              = 113, K_r              = 114, K_s              = 115, K_t              = 116, K_u              = 117,
	K_v              = 118, K_w              = 119, K_x              = 120, K_y              = 121, K_z              = 122,
	K_LEFTBRACE      = 123, K_BAR            = 124, K_RIGHTBRACE     = 125, K_TILDE          = 126, K_DELETE         = 127,
	
	K_KP0            = 256, K_KP1            = 257, K_KP2            = 258, K_KP3            = 259, K_KP4            = 260,
	K_KP5            = 261, K_KP6            = 262, K_KP7            = 263, K_KP8            = 264, K_KP9            = 265,
	K_KP_PERIOD      = 266, K_KP_DIVIDE      = 267, K_KP_MULTIPLY    = 268, K_KP_MINUS       = 269, K_KP_PLUS        = 270,
	K_KP_ENTER       = 271, K_KP_EQUALS      = 272,
	
	K_UP             = 273, K_DOWN           = 274, K_RIGHT          = 275, K_LEFT           = 276, K_INSERT         = 277,
	K_HOME           = 278, K_END            = 279, K_PAGEUP         = 280, K_PAGEDOWN       = 281,
	
	K_F1             = 282, K_F2             = 283, K_F3             = 284, K_F4             = 285, K_F5             = 286,
	K_F6             = 287, K_F7             = 288, K_F8             = 289, K_F9             = 290, K_F10            = 291,
	K_F11            = 292, K_F12            = 293, K_F13            = 294, K_F14            = 295, K_F15            = 296,
	
	K_NUMLOCK        = 300, K_CAPSLOCK       = 301, K_SCROLLOCK      = 302, K_RSHIFT         = 303, K_LSHIFT         = 304,
	K_RCTRL          = 305, K_LCTRL          = 306, K_RALT           = 307, K_LALT           = 308, K_RMETA          = 309,
	K_LMETA          = 310, K_LSUPER         = 311, K_RSUPER         = 312, K_MODE           = 313, K_COMPOSE        = 314,
	
	K_HELP           = 315, K_PRINT          = 316, K_SYSREQ         = 317, K_BREAK          = 318, K_MENU           = 319,
	K_POWER          = 320, K_EURO           = 321, K_UNDO           = 322, K_OEM_102        = 323,
	
	K_LAST,                 K_DUMMY          = INT_MAX /* Ensure sizeof(enum) == sizeof(int) */
};

// If the system can't distinguish between different keyboards, keyboard ID is -1.
// scancode is guaranteed to exist, but k may be K_UNKNOWN if the scancode doesn't correspond to anything.
// Key repeat events will not show up here.
virtual void keys_cb(function<void(int kb_id, int scancode, key_t key, bool down)> cb) = 0;


// If the user started a drag inside the window, but mouse is currently outside, the arguments may be outside the window.
// If the mouse is outside the window and nothing is held, the arguments will be -0x80000000, -0x80000000, 0.
// Buttons: 0x01-left, 0x02-right, 0x04-middle
virtual void mouse_cb(function<void(int x, int y, uint8_t buttons)> cb) = 0;

// TODO:
// - mouse wheel
// - gamepad
// - resize window
// - fullscreen
// - sound
};

#ifdef ARGUIPROT_X11
struct _XDisplay;
typedef struct _XDisplay Display;
struct window_x11_info {
	Display* display;
	int screen;
};
extern struct window_x11_info window_x11;
#endif
#endif
