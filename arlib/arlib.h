// Arlib is my utility library / std:: replacement.
// It exists for several reasons:
// - The C++ standard library types lack easy access to lots of useful functionality, such as
//     splitting a string by linebreaks.
// - std:: is full of strange features and guarantees whose existence increase the constant factor
//     of every operation, even if unused and unnecessary.
// - Dependency management in C++ is cumbersome at best.
// - I care a lot about binary size and easy distribution (no DLLs) on Windows, and including the
//     C++ standard library would often triple the program size.
// - And, most importantly, every feature I implement is a feature I fully understand, so I can
//     debug it, debug other instances of the same protocol or format, know which edge cases are
//     likely to cause bugs (for example to write a test suites, or research potential security
//     vulnerabilities), and appreciate the true complexity of something that seems simple.

//if anyone whines about antivirus, https://arstechnica.com/information-technology/2017/01/antivirus-is-bad/
//  and linked:
//    https://robert.ocallahan.org/2017/01/disable-your-antivirus-software-except.html
//    https://twitter.com/justinschuh/status/802491391121260544
//    https://blog.mozilla.org/nnethercote/2012/02/16/mcafee-is-killing-us/
//    https://googleprojectzero.blogspot.com/2016/06/how-to-compromise-enterprise-endpoint.html
//  and https://blogs.msdn.microsoft.com/oldnewthing/20180615-00/?p=99025

#pragma once
#include "global.h"
#include "linqbase.h"

#include "hash.h"
#include "array.h"
#include "function.h"
#include "string.h"
#include "set.h"
#include "endian.h"
#include "intwrap.h"

#include "base64.h"
#include "bytepipe.h"
#include "bytestream.h"
#include "crc32.h"
#include "file.h"
#include "linq.h"
#include "os.h"
#include "random.h"
#include "stringconv.h"

//no ifdef on this one, it contains some dummy implementations if threads are disabled
#include "thread/thread.h"

#include "bml.h"
#include "html.h"
#include "image.h"
#include "init.h"
#include "json.h"
#include "process.h"
#include "regex.h"
#include "runloop.h"
#include "safeint.h"
#include "serialize.h"
#include "test.h"
#include "zip.h"

#if !defined(ARGUI_NONE) && !defined(ARGUI_WINDOWS) && !defined(ARGUI_GTK3)
#define ARGUI_NONE
#endif
#ifndef ARGUI_NONE
#include "gui/window.h"
#endif

#ifdef ARLIB_OPENGL
#include "opengl/aropengl.h"
#endif

#ifdef ARLIB_WUTF
#include "wutf/wutf.h"
#endif

#ifdef ARLIB_SANDBOX
#include "sandbox/sandbox.h"
#endif

#ifdef ARLIB_SOCKET
#include "socket/socket.h"
#include "dns.h"
#include "http.h"
#include "socks5.h"
#include "websocket.h"
#endif
