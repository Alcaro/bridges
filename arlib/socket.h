#pragma once
#ifndef ARLIB_SOCKET
#error attempt to include disabled sockets header
#endif
#include "runloop2.h"
#include "string.h"
#include "time.h"
#include "bytepipe.h"
#include "endian.h"
#include <unistd.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

class socket2 {
public:
	// This struct represents a sockaddr_in6.
	struct address {
		alignas(uint32_t) char _space[28];
		
		address() { memset(_space, 0, sizeof(_space)); }
		address(bytesr by, uint16_t port = 0); // Input must be 0, 4 or 16 bytes.
		address(cstring str, uint16_t port = 0); // If port is not zero, it may be parsed from the input.
		address(const address&) = default;
		
		operator bool();
		
		// These two will return the address without port.
		bytesr as_bytes(); // 4 or 16 bytes, or 0 if empty.
		string as_str();
		
		struct sockaddr * as_native() { return (struct sockaddr*)_space; }
		
		uint16_t port();
		void set_port(uint16_t newport);
		address& with_port(uint16_t newport) { set_port(newport); return *this; }
		
		// If port is non-null, it may be written. The port should contain a default value.
		static bool parse_ipv4(cstring host, bytesw out, uint16_t* port = nullptr);
		static bool parse_ipv6(cstring host, bytesw out, uint16_t* port = nullptr); // If port is non-null, accepts [::1] and rejects ::1.
		// Result length is guaranteed <= 253. Input can be longer, if it contains a port.
		static cstring parse_domain(cstring host, uint16_t* port = nullptr);
		
		static cstring split_port(cstring host, uint16_t* port);
	};
	
	virtual ~socket2() {}
	
	// Like the recv syscall, can return fewer bytes than requested, including zero. If so, call the function again.
	// Two different parts of the program may await can_recv() and can_send() simultaneously, but only one of each per socket.
	// Asking for zero bytes is undefined behavior. However, it can be returned, and means try again.
	virtual ssize_t recv_sync(bytesw by) = 0;
	virtual ssize_t send_sync(bytesr by) = 0;
	virtual async<void> can_recv() = 0;
	virtual async<void> can_send() = 0;
	
	// These will accept a port from the domain name, if one is provided.
	static async<autoptr<socket2>> create(cstring host, uint16_t port);
#ifdef ARLIB_SSL
	static async<autoptr<socket2>> create_ssl(cstring host, uint16_t port);
#endif
	static async<autoptr<socket2>> create_sslmaybe(bool ssl, cstring host, uint16_t port);
	
	// These ones are low level devices. You probably want the above instead.
	
	// Arlib DNS does not recognize the concept of search domains; it will treat every domain as fully qualified.
	//  Search domains are increasingly incompatible with SSL and other modern practice.
	// Arlib DNS doesn't accept the FQDN marker (trailing period) either;
	//  I've never seen one in practice, not even in search-domain-enabled networks.
	// The domain can be an IP address, and if so, will be resolved synchronously.
	static async<address> dns(cstring domain);
	
	static async<autoptr<socket2>> create(address ip);
#ifdef ARLIB_SSL
	// Whether setup fails or succeeds, the inner socket is consumed and can't be extracted.
	// The domain MUST NOT contain a port component.
	static inline async<autoptr<socket2>> wrap_ssl(autoptr<socket2> inner, cstring domain)
	{
#if defined(ARLIB_SSL_OPENSSL)
		return wrap_ssl_openssl(std::move(inner), domain);
#elif defined(ARLIB_SSL_BEARSSL)
		return wrap_ssl_bearssl(std::move(inner), domain);
#else
#error ssl enabled but all backends disabled
#endif
	}
	
#ifdef ARLIB_SSL_OPENSSL
	static async<autoptr<socket2>> wrap_ssl_openssl(autoptr<socket2> inner, cstring domain);
#endif
#ifdef ARLIB_SSL_BEARSSL
	static async<autoptr<socket2>> wrap_ssl_bearssl(autoptr<socket2> inner, cstring domain);
#endif
#endif
	static async<autoptr<socket2>> wrap_sslmaybe(bool ssl, autoptr<socket2> sock, cstring domain);
	
	// Implementation detail of dns(), to be called by the runloop only.
	static void* dns_create();
	static void dns_destroy(void* dns);
};

// Differences from the normal TCP sockets:
// - UDP is a different protocol, of course.
// - Can't be wrapped into SSL or socks5 or whatever.
// - UDP is connectionless, so creating a socket2_udp is asynchronous.
// - recv and send will either return error or a full packet. They will not split packets, nor return zero bytes.
//    If you use a too small buffer, the packet will be truncated and the return value will be the real length.
// - send is synchronous. If the socket is full, the packet is lost.
#ifdef __unix__
class socket2_udp {
	int fd;
	socket2::address addr;
	
	socket2_udp(int fd, socket2::address addr) : fd(fd), addr(addr) {}
public:
	static autoptr<socket2_udp> create(socket2::address ip);
	
	async<void> can_recv() { return runloop2::await_read(fd); }
	ssize_t recv_sync(bytesw by, socket2::address* sender = nullptr);
	void send(bytesr by);
	
	~socket2_udp() { close(fd); }
};
#endif
#ifdef _WIN32
class socket2_udp {
	SOCKET fd;
	WSAEVENT ev;
	socket2::address addr;
	
	socket2_udp(SOCKET fd, socket2::address addr) : fd(fd), addr(addr) { ev = CreateEvent(NULL, false, false, NULL); }
public:
	static autoptr<socket2_udp> create(socket2::address ip);
	
	async<void> can_recv() { WSAEventSelect(fd, ev, FD_READ); return runloop2::await_handle(ev); }
	ssize_t recv_sync(bytesw by, socket2::address* sender = nullptr);
	void send(bytesr by);
	
	~socket2_udp() { WSACloseEvent(ev); closesocket(fd); }
};
#endif

// The socketlisten is a TCP server.
// Since it can yield multiple sockets, and there's little or no need for state management here, it's not a coroutine; it takes a callback.
class socketlisten {
#ifdef __unix__
	int fd = -1;
	struct waiter_t : public waiter<void, waiter_t> {
		void complete() { container_of<&socketlisten::wait>(this)->on_readable(); }
	} wait;
	socketlisten(int fd, function<void(autoptr<socket2>)> cb);
	void on_readable();
#endif
#ifdef _WIN32
	SOCKET fd = INVALID_SOCKET;
#endif
	function<void(autoptr<socket2>)> cb;
public:
	static autoptr<socketlisten> create(uint16_t port, function<void(autoptr<socket2>)> cb);
	~socketlisten() { close(fd); }
};

// A socketbuf is a convenience wrapper to read structured data from the socket. Ask for N bytes and you get N bytes, no need for loops.
// On the send side, it converts send to memcpy, making it act as if it's synchronous and allowing multiple concurrent writers.
class socketbuf {
	autoptr<socket2> sock;
	bytepipe recv_by;
	
	enum op_t {
		op_none, op_u8,
		op_u16b, op_u16l,
		op_u32b, op_u32l,
		op_u64b, op_u64l,
		op_bytesr, op_bytesr_partial,
		op_line,
	} recv_op
#ifndef ARLIB_OPT
		= op_none
#endif
		;
	size_t recv_bytes; // or max length for op_line
	
	template<typename T>
	struct producer_t : public producer<T, producer_t<T>> {
		// extra declval to work around https://github.com/llvm/llvm-project/issues/55812
		void cancel() { container_of<&socketbuf::recv_prod>((decltype(std::declval<socketbuf>().recv_prod)*)this)->recv_cancel(); }
	};
	variant_raw<producer_t<uint8_t>, producer_t<uint16_t>, producer_t<uint32_t>, producer_t<uint64_t>,
	            producer_t<bytesr>, producer_t<cstring>> recv_prod;
	void recv_cancel()
	{
#ifndef ARLIB_OPT
		recv_op = op_none;
#endif
		recv_wait.cancel();
	}
	
	struct waiter_t : public waiter<void, waiter_t> {
		void complete() { container_of<&socketbuf::recv_wait>(this)->recv_ready(); }
	} recv_wait;
	void recv_ready();
	void recv_complete(size_t prev_size);
	void recv_complete_null();
	
	template<typename T, op_t op>
	async<T> get_async()
	{
#ifndef ARLIB_OPT
		if (this->recv_op != op_none)
			debug_fatal_stack("can't read from a socketbuf twice");
#endif
		this->recv_op = op;
		async<T> ret = recv_prod.construct<producer_t<T>>();
		recv_complete(0);
		return ret;
	}
	template<typename T, op_t op>
	async<T> ret_int()
	{
		recv_bytes = sizeof(T);
		return get_async<T, op>();
	}
public:
	socketbuf() { recv_by.bufsize(4096); }
	socketbuf(autoptr<socket2> sock) : sock(std::move(sock)) { recv_by.bufsize(4096); }
	socketbuf& operator=(autoptr<socket2> sock)
	{
		reset();
		this->sock = std::move(sock);
		return *this;
	}
	socketbuf& operator=(socketbuf&& other)
	{
		other.send_wait.cancel();
		sock = std::move(other.sock);
		recv_by = std::move(other.recv_by);
		send_by = std::move(other.send_by);
		this->send_prepare();
		
		return *this;
	}
	
	void reset()
	{
		sock = nullptr;
		recv_by.reset(4096);
		send_by.reset();
		send_wait.cancel();
#ifndef ARLIB_OPT
		if (recv_op != op_none)
			debug_fatal_stack("can't reset a socketbuf that someone's trying to read");
		if (send_prod.has_waiter())
			debug_fatal_stack("can't reset a socketbuf that someone's trying to write");
#endif
	}
	
	operator bool() { return sock != nullptr; }
	
	// If the socket breaks, these will return the applicable zero value until the object is deleted.
	async<uint8_t> u8() { return ret_int<uint8_t, op_u8>(); }
	async<uint8_t> u8b() { return u8(); }
	async<uint8_t> u8l() { return u8(); }
	async<uint16_t> u16b() { return ret_int<uint16_t, op_u16b>(); }
	async<uint16_t> u16l() { return ret_int<uint16_t, op_u16l>(); }
	async<uint32_t> u32b() { return ret_int<uint32_t, op_u32b>(); }
	async<uint32_t> u32l() { return ret_int<uint32_t, op_u32l>(); }
	async<uint64_t> u64b() { return ret_int<uint64_t, op_u64b>(); }
	async<uint64_t> u64l() { return ret_int<uint64_t, op_u64l>(); }
	
	// The max length is slightly approximate. The object will not read another packet once at least 8192 bytes have been collected,
	//  but if the line is 8200 bytes and the last packet completed it, it will be returned.
	// Length exceeded can be distinguished from socket failure via operator bool, but there's not much you can do with that information.
	// Empty line can, like bytepipe, be distinguished from failure by whether it contains \n.
	async<cstring> line(size_t maxlen = 8192)
	{
		recv_bytes = maxlen;
		return get_async<cstring, op_line>();
	}
	async<bytesr> bytes(size_t n)
	{
		recv_bytes = n;
		return get_async<bytesr, op_bytesr>();
	}
	// Will return anywhere between 1 and n bytes.
	async<bytesr> bytes_partial(size_t n)
	{
		recv_bytes = n;
		return get_async<bytesr, op_bytesr_partial>();
	}
	// Will return anywhere between 0 and n bytes.
	bytesr bytes_sync(size_t n);
	
	async<void> can_recv()
	{
		if (!sock)
			return producer<void>::complete_sync();
		return sock->can_recv();
	}
	
private:
	bytepipe send_by;
	
	struct send_waiter_t : public waiter<void, send_waiter_t> {
		void complete() { container_of<&socketbuf::send_wait>(this)->send_ready(); }
	} send_wait;
	struct send_producer_t : public producer<bool, send_producer_t> {
		void cancel() { container_of<&socketbuf::send_prod>(this)->send_wait.cancel(); }
	} send_prod;
	void send_ready();
	void send_prepare();
public:
	void send(bytesr by);
	void send(cstring str) { return send(str.bytes()); }
	void send(const char * str) { return send(bytesr((uint8_t*)str, strlen(str))); }
	
	// send_buf collects incoming bytes, and does not send them 
	void send_buf(bytesr by) { send_by.push(by); }
	template<typename... Ts> void send_buf(Ts... args) { send_by.push_text(args...); }
	void send_flush() { send_ready(); }
	
	// could make it try to do this synchronously without copying, but no real point
	template<typename... Ts> void send(Ts... args) { send_buf(args...); send_flush(); }
	
	// Completes when the object has nothing left to send.
	// This may be immediately, or may take longer than expected if another coroutine sends something while you're waiting.
	// Only one coroutine may await sends; concurrency here is not allowed.
	async<bool> await_send()
	{
		if (!sock)
			return send_prod.complete_sync(false);
		else if (send_by.size() == 0)
			return send_prod.complete_sync(true);
		else
			return &send_prod;
	}
};
