#pragma once
#include "global.h"
#include <time.h>

#ifdef __unix__
#define DYLIB_EXT ".so"
#define DYLIB_MAKE_NAME(name) "lib" name DYLIB_EXT
#endif
#ifdef _WIN32
#define DYLIB_EXT ".dll"
#define DYLIB_MAKE_NAME(name) name DYLIB_EXT
#endif
#ifdef __GNUC__
#define DLLEXPORT extern "C" __attribute__((__visibility__("default")))
#endif
#ifdef _MSC_VER
#define DLLEXPORT extern "C" __declspec(dllexport)
#endif

class dylib : nocopy {
	void* handle = NULL;
	
public:
	dylib() = default;
	dylib(const char * filename) { init(filename); }
	
	//if called multiple times on the same object, undefined behavior, call deinit() first
	bool init(const char * filename);
	//like init, but if the library is loaded already, it fails
	//does not protect against a subsequent init() loading the same thing
	bool init_uniq(const char * filename);
	//like init_uniq, but if the library is loaded already, it loads a new instance, if supported by the platform
	bool init_uniq_force(const char * filename);
	//guaranteed to return NULL if initialization fails
	void* sym_ptr(const char * name);
	//separate function because
	//  "The ISO C standard does not require that pointers to functions can be cast back and forth to pointers to data."
	//  -- POSIX dlsym, http://pubs.opengroup.org/onlinepubs/009695399/functions/dlsym.html#tag_03_112_08
	//the cast works fine in practice, but why not
	//compiler optimizes it out anyways
	funcptr sym_func(const char * name)
	{
		funcptr ret;
		*(void**)(&ret) = this->sym_ptr(name);
		return ret;
	}
	//TODO: do some decltype shenanigans so dylib.sym<Direct3DCreate9>("Direct3DCreate9") works
	//ideally, dylib.sym(Direct3DCreate9) alone would work, but C++ doesn't offer any way to access the name of a variable
	//except it does. TODO: check if #define sym_named(x) sym<decltype(x)>(#x) is good enough; macros kinda suck, but that name seems rare enough
	template<typename T> T sym(const char * name) { return (T)sym_func(name); }
	
	//Fetches multiple symbols. 'names' is expected to be a NUL-separated list of names, terminated with a blank one.
	// (This is easiest done by using multiple NUL-terminated strings, and let compiler append another NUL.)
	//Returns whether all of them were successfully fetched. If not, failures are NULL; this may not necessarily be all of them.
	bool sym_multi(funcptr* out, const char * names);
	
	void deinit();
	~dylib() { deinit(); }
};

//If the program is run under a debugger, this triggers a breakpoint. If not, does nothing.
//Returns whether it did something. The other three do too, but they always do something, if they return at all.
bool debug_or_ignore();
//If the program is run under a debugger, this triggers a breakpoint. If not, the program whines to stderr, and a nearby file.
bool debug_or_print(const char * filename, int line);
#define debug_or_print() debug_or_print(__FILE__, __LINE__)
//If the program is run under a debugger, this triggers a breakpoint. If not, the program silently exits.
bool debug_or_exit();
//If the program is run under a debugger, this triggers a breakpoint. If not, the program crashes.
bool debug_or_abort();

//Same epoch as time(). They're unsigned because the time is known to be after 1970, but it's fine to cast them to signed.
uint64_t time_ms();
uint64_t time_us(); // this will overflow in year 586524
//No epoch; the epoch may vary across machines or reboots. In exchange, it may be faster.
//ms/us will have the same epoch as each other, and will remain constant unless the machine is rebooted.
//It is unspecified whether this clock ticks while the machine is suspended or hibernated.
uint64_t time_ms_ne();
uint64_t time_us_ne();

class timer {
	uint64_t start;
public:
	timer()
	{
		reset();
	}
	void reset()
	{
		start = time_us_ne();
	}
	uint64_t us()
	{
		return time_us_ne() - start;
	}
	uint64_t ms()
	{
		return us() / 1000;
	}
	uint64_t us_reset()
	{
		uint64_t new_us = time_us_ne();
		uint64_t prev_us = start;
		start = new_us;
		return new_us - prev_us;
	}
	uint64_t ms_reset()
	{
		return us_reset() / 1000;
	}
};

#ifdef _WIN32 // this is safe, gmtime() returns a thread local
#define gmtime_r(a,b) (*(b)=*gmtime(a))
#endif

#ifdef _WIN32 // this function exists on all platforms I've seen
#undef timegm
#define timegm timegm_local
//similar to mktime, but UTC timezone
time_t timegm(struct tm * t);
#endif
