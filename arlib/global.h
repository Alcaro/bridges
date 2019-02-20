#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE //strdup, realpath, asprintf
#endif
#define _strdup strdup //and windows is being windows as usual

//these aren't needed with modern compilers, according to
//https://stackoverflow.com/questions/8132399/how-to-printf-uint64-t-fails-with-spurious-trailing-in-format
//from what I can gather, they're only used for some random ancient MinGW versions
//#define __STDC_LIMIT_MACROS //how many of these stupid things exist
//#define __STDC_FORMAT_MACROS // if I include a header, it's because I want to use its contents
//#define __STDC_CONSTANT_MACROS
#define _USE_MATH_DEFINES // needed for M_PI on Windows

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include "function.h"
#include <utility>

#ifdef _WIN32
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0600
#    define NTDDI_VERSION NTDDI_VISTA
#  elif _WIN32_WINNT < 0x0600
#    undef _WIN32_WINNT
#    define _WIN32_WINNT 0x0502 // 0x0501 excludes SetDllDirectory, so I need to put it at 0x0502
#    define NTDDI_VERSION NTDDI_WS03 // actually NTDDI_WINXPSP2, but MinGW sddkddkver.h gets angry about that
#  endif
#  define _WIN32_IE 0x0600
//the namespace pollution this causes is massive, but without it, there's a bunch of functions that
// just tail call kernel32.dll. With it, they can be inlined.
#  define WIN32_LEAN_AND_MEAN
#  ifndef NOMINMAX
#   define NOMINMAX
#  endif
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
#  ifdef _MSC_VER
#    define _CRT_NONSTDC_NO_DEPRECATE
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  define STRICT
#  include <windows.h>
#  undef STRICT
#endif

#ifdef _MSC_VER
#pragma warning(disable:4800) // forcing value to bool 'true' or 'false' (performance warning)
#endif

#ifndef __has_include
#define __has_include(x) false
#endif

typedef void(*funcptr)();
typedef uint8_t byte; // TODO: remove

#define using(obj) for(bool FIRST=true;FIRST;FIRST=false)for(obj;FIRST;FIRST=false)
//in C++17, this becomes if(obj;true)

#define JOIN_(x, y) x ## y
#define JOIN(x, y) JOIN_(x, y)

#define STR_(x) #x
#define STR(x) STR_(x)

#ifdef __GNUC__
#define LIKELY(expr)    __builtin_expect(!!(expr), true)
#define UNLIKELY(expr)  __builtin_expect(!!(expr), false)
#define MAYBE_UNUSED __attribute__((__unused__)) // shut up, stupid warnings
#define KEEP_OBJECT __attribute__((__used__)) // for static unused variables that shouldn't be optimized out
#else
#define LIKELY(expr)    (expr)
#define UNLIKELY(expr)  (expr)
#define MAYBE_UNUSED
#define KEEP_OBJECT
#define __GNUC__ 0
#endif

//have to include these too, they include <stddef.h> and define NULL to __null, which doesn't resolve overloads properly
#include <string.h>
#ifdef __unix__ // fuck this shit, behave sanely gcc
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <signal.h>
#endif

#undef NULL
#define NULL nullptr

#if __cplusplus < 201103
class nullptr_t_impl {};
#define nullptr_t nullptr_t_impl* // random pointer type nobody will ever use
#else
using std::nullptr_t;
#endif

//some magic stolen from http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx
//C++ can be so messy sometimes...
template<typename T, size_t N> char(&ARRAY_SIZE_CORE(T(&x)[N]))[N];
#define ARRAY_SIZE(x) (sizeof(ARRAY_SIZE_CORE(x)))

//yep, C++ is definitely a mess. based on https://github.com/swansontec/map-macro with some changes:
//- namespaced all child macros, renamed main one
//- merged https://github.com/swansontec/map-macro/pull/3
//- merged http://stackoverflow.com/questions/6707148/foreach-macro-on-macros-arguments#comment62878935_13459454, plus ifdef
#define PPFE_EVAL0(...) __VA_ARGS__
#define PPFE_EVAL1(...) PPFE_EVAL0 (PPFE_EVAL0 (PPFE_EVAL0 (__VA_ARGS__)))
#define PPFE_EVAL2(...) PPFE_EVAL1 (PPFE_EVAL1 (PPFE_EVAL1 (__VA_ARGS__)))
#define PPFE_EVAL3(...) PPFE_EVAL2 (PPFE_EVAL2 (PPFE_EVAL2 (__VA_ARGS__)))
#define PPFE_EVAL4(...) PPFE_EVAL3 (PPFE_EVAL3 (PPFE_EVAL3 (__VA_ARGS__)))
#define PPFE_EVAL(...)  PPFE_EVAL4 (PPFE_EVAL4 (PPFE_EVAL4 (__VA_ARGS__)))
#define PPFE_MAP_END(...)
#define PPFE_MAP_OUT
#define PPFE_MAP_GET_END2() 0, PPFE_MAP_END
#define PPFE_MAP_GET_END1(...) PPFE_MAP_GET_END2
#define PPFE_MAP_GET_END(...) PPFE_MAP_GET_END1
#define PPFE_MAP_NEXT0(test, next, ...) next PPFE_MAP_OUT
#ifdef _MSC_VER
//this version doesn't work on GCC, it makes PPFE_MAP0 not get expanded the second time and quite effectively stops everything.
//but completely unknown guy says it's required on MSVC, so I'll trust that and ifdef it
//pretty sure one of them violate the C99/C++ specifications, but I have no idea which of them, nor what Clang does
#define PPFE_MAP_NEXT1(test, next) PPFE_EVAL0(PPFE_MAP_NEXT0 (test, next, 0))
#else
#define PPFE_MAP_NEXT1(test, next) PPFE_MAP_NEXT0 (test, next, 0)
#endif
#define PPFE_MAP_NEXT(test, next)  PPFE_MAP_NEXT1 (PPFE_MAP_GET_END test, next)
#define PPFE_MAP0(f, x, peek, ...) f(x) PPFE_MAP_NEXT (peek, PPFE_MAP1) (f, peek, __VA_ARGS__)
#define PPFE_MAP1(f, x, peek, ...) f(x) PPFE_MAP_NEXT (peek, PPFE_MAP0) (f, peek, __VA_ARGS__)
#define PPFOREACH(f, ...) PPFE_EVAL (PPFE_MAP1 (f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))
//usage:
//#define STRING(x) const char * x##_string = #x;
//PPFOREACH(STRING, foo, bar, baz)
//limited to 365 entries, but that's enough.



//requirements:
//- static_assert(false) throws something at compile time
//- multiple static_assert(true) works
//- does not require unique names for each assertion
//- zero traces left in the object files, except maybe debug info
//- zero warnings under any compiler
//- static_assert(2+2 < 5); works at the global scope
//- static_assert(2+2 < 5); works as a class member
//- static_assert(2+2 < 5); works inside a function
//- static_assert(2+2 < 5); works in all of the above when templates are involved
//- works on all compilers
//optional:
//- (PASS) works in a template, even if the template isn't instantiated, if the condition isn't dependent on the types
//- (FAIL) works if compiled as C (tried to design an alternate implementation and ifdef it, but nothing works inside structs)
//- (PASS) can name assertions, if desired (only under C++11)
#ifdef __GNUC__
#define TYPENAME_IF_GCC typename // gcc requires this. msvc rejects this.
#else
#define TYPENAME_IF_GCC
#endif

#if __cplusplus < 201703
#if __cplusplus < 201103
template<bool x> struct static_assert_t;
template<> struct static_assert_t<true> { struct STATIC_ASSERTION_FAILED {}; };
template<> struct static_assert_t<false> {};
//#define static_assert(expr)
//	typedef TYPENAME_IF_NEEDED static_assert_t<(bool)(expr)>::STATIC_ASSERTION_FAILED
//	JOIN(static_assertion_, __COUNTER__) MAYBE_UNUSED;
#define static_assert_c(expr, name, ...) \
	enum { \
		JOIN(static_assertion_, __COUNTER__) = \
		sizeof(TYPENAME_IF_GCC static_assert_t<(bool)(expr)>::STATIC_ASSERTION_FAILED) \
	} MAYBE_UNUSED
#else
#define static_assert_c(expr, name, ...) static_assert(expr, name)
#endif

#define static_assert_name(x, ...) #x
#define static_assert(...) static_assert_c(__VA_ARGS__, static_assert_name(__VA_ARGS__))
#endif


//almost C version (fails inside structs)
//#define static_assert(expr) \
//	typedef char JOIN(static_assertion_, __COUNTER__)[(expr)?1:-1]



#ifdef __GNUC__
#define ALIGN(n) __attribute__((aligned(n)))
#endif
#ifdef _MSC_VER
#define ALIGN(n) __declspec(align(n))
#endif




#ifdef __cplusplus
class anyptr {
	void* data;
public:
	template<typename T> anyptr(T* data_) { data = (void*)data_; }
	template<typename T> operator T*() { return (T*)data; }
	template<typename T> operator const T*() const { return (const T*)data; }
};
#else
typedef void* anyptr;
#endif


void malloc_fail(size_t size);

inline anyptr malloc_check(size_t size) { void* ret = malloc(size); if (size && !ret) malloc_fail(size); return ret; }
inline anyptr try_malloc(size_t size) { return malloc(size); }
#define malloc malloc_check

inline anyptr realloc_check(anyptr ptr, size_t size) { void* ret = realloc(ptr, size); if (size && !ret) malloc_fail(size); return ret; }
inline anyptr try_realloc(anyptr ptr, size_t size) { return realloc(ptr, size); }
#define realloc realloc_check

inline anyptr calloc_check(size_t size, size_t count) { void* ret = calloc(size, count); if (size && count && !ret) malloc_fail(size*count); return ret; }
inline anyptr try_calloc(size_t size, size_t count) { return calloc(size, count); }
#define calloc calloc_check

inline void malloc_assert(bool cond) { if (!cond) malloc_fail(0); }


//if I cast it to void, that means I do not care, so shut the hell up about warn_unused_result
template<typename T> static inline void ignore(T t) {}

template<typename T> static T min(const T& a) { return a; }
template<typename T, typename... Args> static T min(const T& a, Args... args)
{
	const T& b = min(args...);
	if (a < b) return a;
	else return b;
}

template<typename T> static T max(const T& a) { return a; }
template<typename T, typename... Args> static T max(const T& a, Args... args)
{
	const T& b = max(args...);
	if (a < b) return b;
	else return a;
}



//too reliant on non-ancient compilers
////some SFINAE shenanigans to call T::create if it exists, otherwise new() - took an eternity to google up
////don't use this template yourself, use generic_create/destroy instead
//template<typename T> class generic_create_core {
//	template<int G> class int_eater {};
//public:
//	template<typename T2> static T* create(T2*, int_eater<sizeof(&T2::create)>*) { return T::create(); }
//	static T* create(T*, ...) { return new T(); }
//	
//	template<typename T2> static void destroy(T* obj, T2*, int_eater<sizeof(&T2::release)>*) { obj->release(); }
//	static void destroy(T* obj, T*, ...) { delete obj; }
//};
//template<typename T> T* generic_create() { return generic_create_core<T>::create((T*)NULL, NULL); }
//template<typename T> void generic_delete(T* obj) { generic_create_core<T>::destroy(obj, (T*)NULL, NULL); }
//
//template<typename T> T* generic_create() { return T::create(); }
//template<typename T> T* generic_new() { return new T; }
//template<typename T> void generic_delete(T* obj) { delete obj; }
//template<typename T> void generic_release(T* obj) { obj->release(); }
//
//template<typename T> void* generic_create_void() { return (void*)generic_create<T>(); }
//template<typename T> void* generic_new_void() { return (void*)generic_new<T>(); }
//template<typename T> void generic_delete_void(void* obj) { generic_delete((T*)obj); }
//template<typename T> void generic_release_void(void* obj) { generic_release((T*)obj); }



//actually, this entire class is useless, just wastes time in gdb
//class empty {
//	//MSVC says error C2503: base classes cannot contain zero-sized arrays
//	//GCC says error: flexible array member 'empty::__zero_size' not at end of 'class whatever'
//	//this is only gonna be used on nonzero objects anyways, and they'll optimize the empty base class
//	
//	//int __zero_size[];
//};

class nocopy {
protected:
	nocopy() {}
	~nocopy() {}
	nocopy(const nocopy&) = delete;
	const nocopy& operator=(const nocopy&) = delete;
#if !defined(_MSC_VER) || _MSC_VER >= 1900 // error C2610: is not a special member function which can be defaulted
                                           // deleting the copies deletes the moves on gcc, but does nothing on msvc2013; known bug
                                           // luckily, those two bugs cancel out pretty well, so we can do this
	nocopy(nocopy&&) = default;
	nocopy& operator=(nocopy&&) = default;
#endif
};

class nomove {
protected:
	nomove() {}
	~nomove() {}
	nomove(const nomove&) = delete;
	const nomove& operator=(const nomove&) = delete;
	nomove(nomove&&) = delete;
	nomove& operator=(nomove&&) = delete;
};

template<typename T>
class autoptr : nocopy {
	T* ptr;
public:
	autoptr() : ptr(NULL) {}
	autoptr(T* ptr) : ptr(ptr) {}
	autoptr(autoptr<T>&& other) { ptr = other.ptr; other.ptr = NULL; }
	autoptr<T>& operator=(T* ptr) { delete this->ptr; this->ptr = ptr; return *this; }
	autoptr<T>& operator=(autoptr<T>&& other) { delete this->ptr; ptr = other.ptr; other.ptr = NULL; return *this; }
	T* release() { T* ret = ptr; ptr = NULL; return ret; }
	T* operator->() { return ptr; }
	T& operator*() { return *ptr; }
	const T* operator->() const { return ptr; }
	const T& operator*() const { return *ptr; }
	operator T*() { return ptr; }
	operator const T*() const { return ptr; }
	explicit operator bool() const { return ptr; }
	~autoptr() { delete ptr; }
};

template<typename T>
class autofree : nocopy {
	T* ptr;
public:
	autofree() : ptr(NULL) {}
	autofree(T* ptr) : ptr(ptr) {}
	autofree(autofree<T>&& other) { ptr = other.ptr; other.ptr = NULL; }
	autofree<T>& operator=(T* ptr) { free(this->ptr); this->ptr = ptr; return *this; }
	autofree<T>& operator=(autofree<T>&& other) { free(this->ptr); ptr = other.ptr; other.ptr = NULL; return *this; }
	T* release() { T* ret = ptr; ptr = NULL; return ret; }
	T* operator->() { return ptr; }
	T& operator*() { return *ptr; }
	const T* operator->() const { return ptr; }
	const T& operator*() const { return *ptr; }
	operator T*() { return ptr; }
	operator const T*() const { return ptr; }
	explicit operator bool() const { return ptr; }
	~autofree() { free(ptr); }
};

template<typename T>
class refcount {
	struct inner_t {
		uint32_t refcount;
		T item;
	};
	inner_t* inner;
	
public:
	refcount() { inner = new inner_t(); inner->refcount = 1; }
	refcount(const refcount<T>& other) { inner = other.inner; inner->refcount++; }
	refcount(refcount<T>&& other) { inner = other.inner; other.inner = NULL; }
	refcount<T>& operator=(T* ptr) = delete;
	refcount<T>& operator=(autofree<T>&& other) = delete;
	T* operator->() { return &inner->item; }
	T& operator*() { return inner->item; }
	const T* operator->() const { return &inner->item; }
	const T& operator*() const { return inner->item; }
	operator T*() { return &inner->item; }
	operator const T*() const { return &inner->item; }
	~refcount() { if (inner && 0 == --inner->refcount) delete inner; }
};

template<typename T>
class iterwrap {
	T b;
	T e;
	
public:
	iterwrap(T b, T e) : b(b), e(e) {}
	template<typename T2> iterwrap(T2& c) : b(c.begin()), e(c.end()) {}
	template<typename T2> iterwrap(const T2& c) : b(c.begin()), e(c.end()) {}
	
	T begin() { return b; }
	T end() { return e; }
};



//if an object should contain callbacks that can destroy the object, you should use the macros below these classes
class destructible {
	friend class destructible_lock;
	bool* pb = NULL;
public:
	~destructible() { if (pb) *pb = true; }
};
class destructible_lock {
	bool b = false;
	destructible* parent;
	bool* prev_pb;
public:
	destructible_lock(destructible& other)
	{
		parent = &other;
		prev_pb = parent->pb;
		parent->pb = &b;
	}
	bool destroyed() { return b; }
	~destructible_lock()
	{
		if (!b) parent->pb = prev_pb;
		else if (prev_pb) *prev_pb = true; // if 'b', 'parent' is poisoned
	}
};
#define MAKE_DESTRUCTIBLE_FROM_CALLBACK() destructible destructible_i
#define RETURN_IF_CALLBACK_DESTRUCTS(op, ...) \
	do { \
		destructible_lock destructible_lock_i(this->destructible_i); \
		{ op; } \
		if (destructible_lock_i.destroyed()) \
			return __VA_ARGS__; \
	} while(0)
//why? easier than doing it manually, and supports exceptions




//#if defined(__linux__) || GCC_VERSION >= 40900
//#define asprintf(...) malloc_assert(asprintf(__VA_ARGS__) >= 0)
//#else
//void asprintf(char * * ptr, const char * fmt, ...);
//#endif

//Acts like strstr, with the obvious difference.
#ifdef _WIN32 // linux has this already
void* memmem(const void * haystack, size_t haystacklen, const void * needle, size_t needlelen);
#endif
//Returns distance to first difference, or 'len' if that's smaller.
size_t memcmp_d(const void * a, const void * b, size_t len);


//msvc:
//typedef unsigned long uint32_t;
//typedef unsigned __int64 uint64_t;
//typedef unsigned int size_t;

//undefined behavior if 'in' is 0 or negative, or if the output would be out of range
template<typename T> static inline T bitround(T in)
{
#if defined(__GNUC__) && defined(__x86_64__)
	//this seems somewhat faster, and more importantly, it's smaller
	//x64 only because I don't know if it does something stupid on other archs
	//I'm surprised gcc doesn't detect this pattern and optimize it, it does detect a few others (like byteswap)
	//special casing every size is because if T is signed, ~(T)0 is -1, and -1 >> N is -1 (and likely also undefined behavior)
	if (in == 1) return in;
	if (sizeof(T) == 1) return 1 + ((~(uint8_t )0) >> __builtin_clz(in - 1));
	if (sizeof(T) == 2) return 1 + ((~(uint16_t)0) >> __builtin_clz(in - 1));
	if (sizeof(T) == 4) return 1 + ((~(uint32_t)0) >> __builtin_clz(in - 1));
	if (sizeof(T) == 8) return 1 + ((~(uint64_t)0) >> __builtin_clzll(in-1));
	abort();
	
#endif
	//TODO: https://stackoverflow.com/q/355967 for msvc
	in--;
	in |= in>>1;
	in |= in>>2;
	in |= in>>4;
	in |= (sizeof(in)>1 ?  8 : 0); // ?: rather than if() to avoid 'shift amount out of range' warnings if the condition is false
	in |= (sizeof(in)>2 ? 16 : 0); // may be gcc bug, but let's just work around it
	in |= (sizeof(in)>4 ? 32 : 0);
	in++;
	return in;
}


#define ALLINTS(x) \
	x(signed char) \
	x(unsigned char) \
	x(signed short) \
	x(unsigned short) \
	x(signed int) \
	x(unsigned int) \
	x(signed long) \
	x(unsigned long) \
	x(signed long long) \
	x(unsigned long long)

#define ALLNUMS(x) \
	ALLINTS(x) \
	x(float) \
	x(double) \

#ifndef COMMON_INST
#define COMMON_INST(T) extern template class T
#endif

//For cases where Gcc thinks a variable is used uninitialized, but it isn't in practice.
//Usage: int foo KNOWN_INIT(0), where argument is any valid value for that type
#define KNOWN_INIT(x) = x


//If an interface defines a function to set some state, and a callback for when this state changes,
// calling that function will not trigger the state callback.
//An implementation may, at its sole discretion, choose to define any implementation of undefined
// behaviour. After all, any result, including something well defined, is a valid interpretation of
// undefined behaviour. The user may, of course, not rely on that.
//Any function that starts with an underscore may only be called by the module that implements that
// function. ("Module" is defined as "anything whose compilation is controlled by the same #ifdef,
// or the file implementing an interface, whichever makes sense"; for example, window-win32-* is the
// same module.) The arguments and return values of these private functions may change meaning
// between modules, and the functions are not guaranteed to exist at all, or closely correspond to
// their name. For example, _window_init_misc on GTK+ instead initializes a component needed by the
// listboxes.

//This file, and many other parts of Arlib, uses a weird mix between Windows- and Linux-style
// filenames and paths. This is intentional; the author prefers Linux-style paths and directory
// structures, but Windows file extensions. .exe is less ambigous than no extension, and 'so' is a
// word while 'dll' is not; however, Windows' insistence on overloading the escape character is
// irritating. Since this excludes following any single OS, the rest is personal preference.
