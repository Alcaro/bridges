#if defined(ARLIB_SOCKET)
#include "socket.h"
#undef socket
#endif

#if defined(ARLIB_SOCKET) && defined(__unix__)
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

static int mksocket(int domain, int type, int protocol)
{
	return socket(domain, type, protocol);
}

namespace {

static int fixret(int ret)
{
	if (ret > 0) return ret;
	if (ret == 0) return -1;
	if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
	return -1;
}

static int my_setsockopt(int fd, int level, int option_name, const void * option_value, socklen_t option_len)
{
	return ::setsockopt(fd, level, option_name, (char*)/*lol windows*/option_value, option_len);
}
static int my_setsockopt(int fd, int level, int option_name, int option_value)
{
	return my_setsockopt(fd, level, option_name, &option_value, sizeof(option_value));
}
#define setsockopt my_setsockopt

static void configure_fd(int fd)
{
	//because 30 second pauses are unequivocally detestable
	timeval timeout;
	timeout.tv_sec = 4;
	timeout.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)); // strace calls this SO_SNDTIMEO_OLD, can't find any docs
	// TODO: do I need the above at all? It made sense when connect() was blocking, but it no longer is.
	
	// Nagle's algorithm - made sense in the 80s, but these days, the saved bandwidth isn't worth the added latency.
	// Better combine stuff in userspace, syscalls are (relatively) expensive these days.
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, true);
	
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, 1); // enable
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, 3); // ping count before the kernel gives up
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, 30); // seconds idle until it starts pinging
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, 10); // seconds per ping once the pings start
}

//MSG_DONTWAIT is usually better, but accept() and connect() don't take that argument
static void MAYBE_UNUSED setblock(int fd, bool newblock)
{
	int flags = fcntl(fd, F_GETFL, 0);
	flags &= ~O_NONBLOCK;
	if (!newblock) flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);
}

class socket2_impl : public socket2 {
public:
	int fd;
	
	socket2_impl(int fd) : fd(fd) {}
	
	ssize_t recv_sync(bytesw by) override
	{
		return fixret(::recv(fd, (char*)by.ptr(), by.size(), MSG_DONTWAIT|MSG_NOSIGNAL));
	}
	ssize_t send_sync(bytesr by) override
	{
		return fixret(::send(fd, (char*)by.ptr(), by.size(), MSG_DONTWAIT|MSG_NOSIGNAL));
	}
	async<void> can_recv() override
	{
		return runloop2::await_read(fd);
	}
	async<void> can_send() override
	{
		return runloop2::await_write(fd);
	}
	
	~socket2_impl() { close(fd); }
};
}

async<autoptr<socket2>> socket2::create(address addr)
{
	if (!addr)
		co_return nullptr;
	
	int fd = mksocket(addr.as_native()->sa_family, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
	if (fd < 0)
		co_return nullptr;
	
	configure_fd(fd);
	
	if (connect(fd, addr.as_native(), sizeof(addr)) != 0 && errno != EINPROGRESS)
	{
	fail:
		close(fd);
		co_return nullptr;
	}
	
	co_await runloop2::await_write(fd);
	
	int error = 0;
	socklen_t len = sizeof(error);
	getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
	if (error != 0)
		goto fail;
	
	co_return new socket2_impl(fd);
}


autoptr<socket2_udp> socket2_udp::create(socket2::address ip)
{
	if (!ip)
		return nullptr;
	int fd = mksocket(ip.as_native()->sa_family, SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
	if (fd < 0)
		return nullptr;
	return new socket2_udp(fd, ip);
}

ssize_t socket2_udp::recv_sync(bytesw by, socket2::address* sender)
{
	socklen_t addrsize = sender ? sizeof(*sender) : 0;
	return fixret(::recvfrom(fd, by.ptr(), by.size(), MSG_DONTWAIT|MSG_NOSIGNAL, sender->as_native(), &addrsize));
}

void socket2_udp::send(bytesr by)
{
	::sendto(fd, by.ptr(), by.size(), MSG_DONTWAIT|MSG_NOSIGNAL, addr.as_native(), sizeof(addr));
}


autoptr<socketlisten> socketlisten::create(uint16_t port, function<void(autoptr<socket2>)> cb)
{
	struct sockaddr_in6 sa; // IN6ADDR_ANY_INIT should work, but doesn't.
	memset(&sa, 0, sizeof(sa));
	sa.sin6_family = AF_INET6;
	sa.sin6_addr = in6addr_any;
	sa.sin6_port = htons(port);
	
	int fd = mksocket(AF_INET6, SOCK_STREAM, 0);
	if (fd < 0) return nullptr;
	
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, false) < 0) goto fail;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, true) < 0) goto fail;
	if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) goto fail;
	if (listen(fd, 10) < 0) goto fail;
	return new socketlisten(fd, std::move(cb));
	
fail:
	close(fd);
	return nullptr;
}

socketlisten::socketlisten(int fd, function<void(autoptr<socket2>)> cb) : fd(fd), cb(std::move(cb))
{
	setblock(fd, false);
	runloop2::await_read(fd).then(&wait);
}

void socketlisten::on_readable()
{
#ifdef __linux__
	int nfd = accept4(this->fd, NULL, NULL, SOCK_CLOEXEC);
#else
	int nfd = accept(this->fd, NULL, NULL);
#endif
	if (nfd >= 0)
	{
		configure_fd(nfd);
		this->cb(new socket2_impl(nfd));
	}
	runloop2::await_read(fd).then(&wait);
}
#endif


#ifdef ARLIB_SOCKET
// no need to copy these to the windows side
void socketbuf::recv_ready()
{
	ssize_t n = sock->recv_sync(recv_by.push_begin(1024));
	if (n < 0)
		sock = nullptr;
	if (!sock)
		return recv_complete_null();
	size_t prev_size = recv_by.size();
	recv_by.push_finish(n);
	recv_complete(prev_size);
}
void socketbuf::recv_complete(size_t prev_size)
{
	if (!sock)
		return recv_complete_null();
	if (recv_op == op_line)
	{
		bytesr line = recv_by.pull_line(prev_size);
		if (line || recv_by.size() >= recv_bytes)
		{
#ifndef ARLIB_OPT
			this->recv_op = op_none;
#endif
			recv_prod.get<producer_t<cstring>>().complete(line);
		}
		else
			sock->can_recv().then(&recv_wait);
	}
	else if (recv_op == op_bytesr_partial && recv_by.size() >= 1)
	{
#ifndef ARLIB_OPT
		this->recv_op = op_none;
#endif
		recv_prod.get<producer_t<bytesr>>().complete(recv_by.pull(min(recv_bytes, recv_by.size())));
	}
	else if (recv_by.size() >= recv_bytes)
	{
		op_t op = this->recv_op;
#ifndef ARLIB_OPT
		this->recv_op = op_none;
#endif
		// don't bother destructing these guys, their dtor is empty anyways (other than an assert)
		if (op == op_u8) recv_prod.get<producer_t<uint8_t>>().complete(readu_le8(recv_by.pull(1).ptr()));
		else if (op == op_u16b) recv_prod.get<producer_t<uint16_t>>().complete(readu_be16(recv_by.pull(2).ptr()));
		else if (op == op_u16l) recv_prod.get<producer_t<uint16_t>>().complete(readu_le16(recv_by.pull(2).ptr()));
		else if (op == op_u32b) recv_prod.get<producer_t<uint32_t>>().complete(readu_be32(recv_by.pull(4).ptr()));
		else if (op == op_u32l) recv_prod.get<producer_t<uint32_t>>().complete(readu_le32(recv_by.pull(4).ptr()));
		else if (op == op_u64b) recv_prod.get<producer_t<uint64_t>>().complete(readu_be64(recv_by.pull(8).ptr()));
		else if (op == op_u64l) recv_prod.get<producer_t<uint64_t>>().complete(readu_le64(recv_by.pull(8).ptr()));
		else if (op == op_bytesr) recv_prod.get<producer_t<bytesr>>().complete(recv_by.pull(recv_bytes));
		else __builtin_unreachable();
	}
	else
	{
		sock->can_recv().then(&recv_wait);
	}
}
void socketbuf::recv_complete_null()
{
	op_t op = this->recv_op;
#ifndef ARLIB_OPT
	this->recv_op = op_none;
#endif
	// these branches yield identical machine code and should be merged
	// (they won't)
	if (op == op_u8)
		recv_prod.get<producer_t<uint8_t>>().complete(0);
	else if (op <= op_u16l)
		recv_prod.get<producer_t<uint16_t>>().complete(0);
	else if (op <= op_u32l)
		recv_prod.get<producer_t<uint32_t>>().complete(0);
	else if (op <= op_u64l)
		recv_prod.get<producer_t<uint64_t>>().complete(0);
	else if (op == op_bytesr)
		recv_prod.get<producer_t<bytesr>>().complete(nullptr);
	else if (op == op_line)
		recv_prod.get<producer_t<cstring>>().complete("");
	else
		__builtin_unreachable();
}

bytesr socketbuf::bytes_sync(size_t n)
{
	if (!sock)
		return {};
	bytesr by1 = recv_by.pull_begin(1);
	if (by1)
	{
		n = min(n, by1.size());
		recv_by.pull_finish(n);
		return by1.slice(0, n);
	}
	bytesw by2 = recv_by.push_begin(1);
	if (by2.size() > n)
		by2 = by2.slice(0, n);
	ssize_t amt = sock->recv_sync(by2);
	if (amt < 0)
	{
		sock = nullptr;
		return {};
	}
	return by2.slice(0, amt);
}

void socketbuf::send_ready()
{
	ssize_t n = sock->send_sync(send_by.pull_begin());
	if (n < 0)
		sock = nullptr;
	if (!sock)
	{
		if (send_prod.has_waiter())
			send_prod.complete(false);
		return;
	}
	send_by.pull_finish(n);
	send_prepare();
}
void socketbuf::send_prepare()
{
	if (send_by.size())
	{
		sock->can_send().then(&send_wait);
	}
	else
	{
		if (send_prod.has_waiter())
			send_prod.complete(true);
	}
}
void socketbuf::send(bytesr by)
{
	if (!sock)
		return;
	if (send_by.size() == 0)
	{
		ssize_t n = sock->send_sync(by);
		if (n < 0)
		{
			sock = nullptr;
			return;
		}
		by = by.skip(n);
		if (!by)
			return;
	}
	send_by.push(by);
	send_prepare();
}
#endif
