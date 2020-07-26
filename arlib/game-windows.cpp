#if defined(ARLIB_GAME) && defined(_WIN32)
#include "game.h"
#include "runloop.h"
#include <windowsx.h> // GET_X_LPARAM

#define WS_BASE WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX // okay microsoft, did I miss anything?
#define WS_RESIZABLE (WS_BASE|WS_MAXIMIZEBOX|WS_THICKFRAME)
#define WS_NONRESIZ (WS_BASE|WS_BORDER)

class gameview_windows : public gameview {
public:

HWND parent;
HWND child;

gameview_windows(uint32_t width, uint32_t height, cstring windowtitle, uintptr_t* pparent, uintptr_t** pchild)
{
	static_assert(sizeof(HWND) == sizeof(uintptr_t));
	
	WNDCLASS wc;
	wc.style = 0;
	wc.lpfnWndProc = DefWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "arlib";
	RegisterClass(&wc);
	
	parent = CreateWindow("arlib", windowtitle.c_str().c_str(), WS_NONRESIZ,
	                      CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, NULL, NULL);
	SetWindowLongPtr(parent, GWLP_USERDATA, (LONG_PTR)this);
	SetWindowLongPtr(parent, GWLP_WNDPROC, (LONG_PTR)s_WindowProc);
	
	// if I set a size, it's because I want that size as client area.
	// nobody cares about total window size, especially in XP+ where window borders are rounded
	RECT inner;
	RECT outer;
	GetClientRect(parent, &inner);
	GetWindowRect(parent, &outer);
	uint32_t borderwidth = outer.right - outer.left - inner.right;
	uint32_t borderheight = outer.bottom - outer.top - inner.bottom;
	SetWindowPos(parent, NULL, 0, 0, width+borderwidth, height+borderheight,
	             SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
	
	ShowWindow(parent, SW_SHOWNORMAL);
	
	*pparent = (uintptr_t)parent;
	*pchild = (uintptr_t*)&child;
	
	calc_keyboard_map();
}

bool stopped = false;
function<void()> cb_exit = [this](){ stop(); };
/*public*/ bool running() { return !stopped; }
/*public*/ void stop() { stopped = true; }
/*public*/ void exit_cb(function<void()> cb) { cb_exit = cb; }

/*public*/ bool focused() { return (GetForegroundWindow() == parent); }

~gameview_windows()
{
	DestroyWindow(this->parent);
}



key_t vk_to_key(unsigned vk)
{
	static const uint8_t vk_to_key_raw[256] = { // there are only 256 vks, just hardcode it
		0,   0,   0,   0,   0,   0,   0,   0,   0x08,0x09,0,   0,   0x0C,0x0D,0,   0,
		0,   0,   0,   0x13,0xAD,0,   0,   0,   0,   0,   0,   0x1B,0,   0,   0,   0,
		0x20,0x98,0x99,0x97,0x96,0x94,0x91,0x93,0x92,0,   0,   0,   0xBC,0x95,0x7F,0,
		0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0,   0,   0,   0,   0,   0,
		0,   0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
		0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0,   0,   0,   0,   0xC0,
		0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8C,0x8E,0,   0x8D,0x8A,0x8B,
		0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0xAC,0xAE,0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0xB0,0xAF,0xB2,0xB1,0xB4,0xB3,0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0x2B,0x2C,0x2D,0x2E,0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0xC3,0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	};
	unsigned ret = vk_to_key_raw[vk];
	ret += (ret&128);
	return (key_t)ret;
}
function<void(int kb_id, int scancode, key_t k, bool down)> cb_keys;

void calc_keyboard_map()
{
#if 0
	struct {
		uint8_t arlib;
		uint8_t vk;
	} static const keymap[] = {
#define MAP(arlib, vk) { (arlib&256)>>1 | (arlib&127), vk },
		MAP(K_BACKSPACE, VK_BACK) MAP(K_TAB, VK_TAB) MAP(K_CLEAR, VK_CLEAR) MAP(K_RETURN, VK_RETURN) MAP(K_PAUSE, VK_PAUSE)
		MAP(K_ESCAPE, VK_ESCAPE) MAP(K_SPACE, VK_SPACE) /*{ K_EXCLAIM,  },*/ /*{ K_QUOTEDBL,  },*/ /*{ K_HASH,  },*/
		/*{ K_DOLLAR,  },*/ /*{ K_AMPERSAND,  },*/ /*{ K_QUOTE,  },*/ /*{ K_LEFTPAREN,  },*/ /*{ K_RIGHTPAREN,  },*/
		/*{ K_ASTERISK,  },*/ MAP(K_PLUS, VK_OEM_PLUS) MAP(K_COMMA, VK_OEM_COMMA) MAP(K_MINUS, VK_OEM_MINUS)
		MAP(K_PERIOD, VK_OEM_PERIOD) /*{ K_SLASH,  },*/
		MAP(K_0, '0') MAP(K_1, '1') MAP(K_2, '2') MAP(K_3, '3') MAP(K_4, '4')
		MAP(K_5, '5') MAP(K_6, '6') MAP(K_7, '7') MAP(K_8, '8') MAP(K_9, '9')
		/*{ K_COLON,  },*/ /*{ K_SEMICOLON,  },*/ /*{ K_LESS,  },*/ /*{ K_EQUALS,  },*/ /*{ K_GREATER,  },*/ /*{ K_QUESTION,  },*/
		/*{ K_AT,  },*/ /*{ K_LEFTBRACKET,  },*/ /*{ K_BACKSLASH,  },*/ /*{ K_RIGHTBRACKET,  },*/ /*{ K_CARET,  },*/
		/*{ K_UNDERSCORE,  },*/ /*{ K_BACKQUOTE,  },*/
		MAP(K_a, 'A') MAP(K_b, 'B') MAP(K_c, 'C') MAP(K_d, 'D') MAP(K_e, 'E') MAP(K_f, 'F') MAP(K_g, 'G')
		MAP(K_h, 'H') MAP(K_i, 'I') MAP(K_j, 'J') MAP(K_k, 'K') MAP(K_l, 'L') MAP(K_m, 'M') MAP(K_n, 'N')
		MAP(K_o, 'O') MAP(K_p, 'P') MAP(K_q, 'Q') MAP(K_r, 'R') MAP(K_s, 'S') MAP(K_t, 'T') MAP(K_u, 'U')
		MAP(K_v, 'V') MAP(K_w, 'W') MAP(K_x, 'X') MAP(K_y, 'Y') MAP(K_z, 'Z')
		MAP(K_DELETE, VK_DELETE)
		
		MAP(K_KP0, VK_NUMPAD0) MAP(K_KP1, VK_NUMPAD1) MAP(K_KP2, VK_NUMPAD2) MAP(K_KP3, VK_NUMPAD3) MAP(K_KP4, VK_NUMPAD4)
		MAP(K_KP5, VK_NUMPAD5) MAP(K_KP6, VK_NUMPAD6) MAP(K_KP7, VK_NUMPAD7) MAP(K_KP8, VK_NUMPAD8) MAP(K_KP9, VK_NUMPAD9)
		MAP(K_KP_PERIOD, VK_DECIMAL) MAP(K_KP_DIVIDE, VK_DIVIDE) MAP(K_KP_MULTIPLY, VK_MULTIPLY)
		MAP(K_KP_MINUS, VK_SUBTRACT) MAP(K_KP_PLUS, VK_ADD) /*{ K_KP_ENTER,  },*/ /*{ K_KP_EQUALS,  },*/
		
		MAP(K_UP, VK_UP) MAP(K_DOWN, VK_DOWN) MAP(K_RIGHT, VK_RIGHT) MAP(K_LEFT, VK_LEFT)
		MAP(K_INSERT, VK_INSERT) MAP(K_HOME, VK_HOME) MAP(K_END, VK_END) MAP(K_PAGEUP, VK_PRIOR) MAP(K_PAGEDOWN, VK_NEXT)
		
		MAP(K_F1, VK_F1)   MAP(K_F2, VK_F2)   MAP(K_F3, VK_F3)   MAP(K_F4, VK_F4)   MAP(K_F5, VK_F5)
		MAP(K_F6, VK_F6)   MAP(K_F7, VK_F7)   MAP(K_F8, VK_F8)   MAP(K_F9, VK_F9)   MAP(K_F10, VK_F10)
		MAP(K_F11, VK_F11) MAP(K_F12, VK_F12) MAP(K_F13, VK_F13) MAP(K_F14, VK_F14) MAP(K_F15, VK_F15)
		
		MAP(K_NUMLOCK, VK_NUMLOCK) MAP(K_CAPSLOCK, VK_CAPITAL) MAP(K_SCROLLOCK, VK_SCROLL)
		MAP(K_RSHIFT, VK_RSHIFT) MAP(K_LSHIFT, VK_LSHIFT) MAP(K_RCTRL, VK_RCONTROL) MAP(K_LCTRL, VK_LCONTROL)
		MAP(K_RALT, VK_RMENU) MAP(K_LALT, VK_LMENU) /*{ K_RMETA,  },*/ /*{ K_LMETA,  },*/
		/*{ K_LSUPER,  },*/ /*{ K_RSUPER,  },*/ /*{ K_MODE,  },*/ /*{ K_COMPOSE,  },*/
		
		/*{ K_HELP,  },*/ MAP(K_PRINT, VK_SNAPSHOT) /*{ K_SYSREQ,  },*/
		/*{ K_BREAK,  },*/ /*{ K_MENU,  },*/ MAP(K_POWER, VK_SLEEP)
		/*{ K_EURO,  },*/ /*{ K_UNDO,  },*/ MAP(K_OEM_102, VK_OEM_102)
#undef MAP
	};
	
	uint8_t n[256] = {};
	for (size_t i : range(ARRAY_SIZE(keymap))) n[keymap[i].vk] = keymap[i].arlib;
	puts(tostringhex(n));
#endif
}

/*public*/ void keys_cb(function<void(int kb_id, int scancode, key_t k, bool down)> cb)
{
	cb_keys = cb;
}



function<void(int x, int y, uint8_t buttons)> cb_mouse;

/*public*/ void mouse_cb(function<void(int x, int y, uint8_t buttons)> cb)
{
	cb_mouse = cb;
	SetWindowLongPtr(child, GWLP_USERDATA, (LONG_PTR)this); // can't do this in constructor, child is only set by ctx-windows.cpp
	SetWindowLongPtr(child, GWLP_WNDPROC, (LONG_PTR)s_WindowProc);
}



static LRESULT CALLBACK s_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	gameview_windows * This=(gameview_windows*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	return This->WindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE: cb_exit(); break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		if ((lParam&0xC0000000) != 0x40000000) // repeat
			cb_keys(-1, (lParam>>16)&255, vk_to_key(wParam), !(lParam&0x80000000));
		break;
	case WM_LBUTTONUP:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MOUSEMOVE:
		{
			TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, child, HOVER_DEFAULT };
			TrackMouseEvent(&tme);
			cb_mouse(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (wParam&(MK_LBUTTON|MK_RBUTTON)) | (wParam&MK_MBUTTON)>>2);
		}
		break;
	case WM_MOUSELEAVE: cb_mouse(-1, -1, 0); break;
	default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}



/*public*/ void tmp_step(bool wait) { runloop::global()->step(wait); }

};

gameview* gameview::create(uint32_t width, uint32_t height, cstring windowtitle, uintptr_t* parent, uintptr_t** child) {
	return new gameview_windows(width, height, windowtitle, parent, child); }

void _window_process_events();
void _window_process_events()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

#endif
