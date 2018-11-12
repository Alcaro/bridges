#ifdef ARLIB_SOCKET
#include "socket.h"
#include "../stringconv.h"

string socket::ip_to_string(arrayview<byte> ip)
{
	if (ip.size() == 4)
	{
		return tostring(ip[0])+"."+tostring(ip[1])+"."+tostring(ip[2])+"."+tostring(ip[3]);
	}
/*
TODO
https://tools.ietf.org/html/rfc5952

4.  A Recommendation for IPv6 Text Representation

   A recommendation for a canonical text representation format of IPv6
   addresses is presented in this section.  The recommendation in this
   document is one that complies fully with [RFC4291], is implemented by
   various operating systems, and is human friendly.  The recommendation
   in this section SHOULD be followed by systems when generating an
   address to be represented as text, but all implementations MUST
   accept and be able to handle any legitimate [RFC4291] format.  It is
   advised that humans also follow these recommendations when spelling
   an address.

4.1.  Handling Leading Zeros in a 16-Bit Field

   Leading zeros MUST be suppressed.  For example, 2001:0db8::0001 is
   not acceptable and must be represented as 2001:db8::1.  A single 16-
   bit 0000 field MUST be represented as 0.

4.2.  "::" Usage

4.2.1.  Shorten as Much as Possible

   The use of the symbol "::" MUST be used to its maximum capability.
   For example, 2001:db8:0:0:0:0:2:1 must be shortened to 2001:db8::2:1.
   Likewise, 2001:db8::0:1 is not acceptable, because the symbol "::"
   could have been used to produce a shorter representation 2001:db8::1.

4.2.2.  Handling One 16-Bit 0 Field

   The symbol "::" MUST NOT be used to shorten just one 16-bit 0 field.
   For example, the representation 2001:db8:0:1:1:1:1:1 is correct, but
   2001:db8::1:1:1:1:1 is not correct.

4.2.3.  Choice in Placement of "::"

   When there is an alternative choice in the placement of a "::", the
   longest run of consecutive 16-bit 0 fields MUST be shortened (i.e.,
   the sequence with three consecutive zero fields is shortened in 2001:
   0:0:1:0:0:0:1).  When the length of the consecutive 16-bit 0 fields
   are equal (i.e., 2001:db8:0:0:1:0:0:1), the first sequence of zero
   bits MUST be shortened.  For example, 2001:db8::1:0:0:1 is correct
   representation.

4.3.  Lowercase

   The characters "a", "b", "c", "d", "e", and "f" in an IPv6 address
   MUST be represented in lowercase.

*/
	return "";
}


array<byte> socket::string_to_ip(cstring str)
{
	uint8_t out[16];
	return array<byte>(out, string_to_ip(out, str));
}

int socket::string_to_ip(arrayvieww<byte> out, cstring str)
{
	if (out.size() < 16) return 16;
	if (string_to_ip4(out, str)) return 4;
	if (string_to_ip6(out, str)) return 16;
	return 0;
}

bool socket::string_to_ip4(arrayvieww<byte> out, cstring str)
{
	const char* inp = (char*)str.bytes().ptr();
	const char* inpe = inp + str.length();
	
	int outp = 0;
	
	while (true)
	{
		char tmp[4];
		int tmpp = 0;
		while (*inp != '.' && inp < inpe)
		{
			if (tmpp == 3) return false;
			tmp[tmpp++] = *inp;
			inp++;
		}
		tmp[tmpp] = '\0';
		
		if (tmpp > 1 && tmp[0] == '0') return false;
		if (!fromstring(tmp, out[outp++])) return false;
		if (inp == inpe) return (outp == 4);
		inp++;
	}
}

bool socket::string_to_ip6(arrayvieww<byte> out, cstring str) { return false; }

#include "../test.h"
test("IP conversion", "array,string", "ipconv")
{
	assert_eq(tostringhex(socket::string_to_ip("127.0.0.1")), "7F000001");
	assert_eq(tostringhex(socket::string_to_ip("127.0.0.")), "");
	assert_eq(tostringhex(socket::string_to_ip("0127.0.0.1")), "");
	assert_eq(tostringhex(socket::string_to_ip("127.00.0.1")), "");
	assert_eq(tostringhex(socket::string_to_ip("127.256.0.1")), "");
	assert_eq(tostringhex(socket::string_to_ip(".0.0.1")), "");
	assert_eq(tostringhex(socket::string_to_ip("127.0.0")), "");
	
	assert_eq(socket::ip_to_string(socket::string_to_ip("127.0.0.1")), "127.0.0.1");
}
#endif
