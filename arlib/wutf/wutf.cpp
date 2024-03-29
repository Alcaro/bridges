// SPDX-License-Identifier: MIT
// The above license applies only to the WuTF directory, not the entire Arlib.

//See wutf.h for documentation.

#ifdef _WIN32
#include "wutf.h"
#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#include <stdio.h>

#ifndef NTSTATUS
#define NTSTATUS LONG
#endif
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS 0x00000000
#endif

static decltype(RtlAllocateHeap)* RtlAllocateHeap_ptr = NULL;

static NTSTATUS WINAPI
RtlMultiByteToUnicodeN_Utf(
                PWCH   UnicodeString,
                ULONG  MaxBytesInUnicodeString,
                PULONG BytesInUnicodeString,
          const CHAR   *MultiByteString,
                ULONG  BytesInMultiByteString)
{
	int len = WuTF_utf8_to_utf16(WUTF_TRUNCATE | WUTF_INVALID_DROP,
	                             MultiByteString, BytesInMultiByteString,
	                             (uint16_t*)UnicodeString, MaxBytesInUnicodeString/2);
	if (BytesInUnicodeString) *BytesInUnicodeString = len*2;
	return STATUS_SUCCESS;
}

static NTSTATUS WINAPI
RtlUnicodeToMultiByteN_Utf(
                PCHAR  MultiByteString,
                ULONG  MaxBytesInMultiByteString,
                PULONG BytesInMultiByteString,
                PCWCH  UnicodeString,
                ULONG  BytesInUnicodeString)
{
	int len = WuTF_utf16_to_utf8(WUTF_TRUNCATE | WUTF_INVALID_DROP,
	                             (uint16_t*)UnicodeString, BytesInUnicodeString/2,
	                             MultiByteString, MaxBytesInMultiByteString);
	if (BytesInMultiByteString) *BytesInMultiByteString = len;
	return STATUS_SUCCESS;
}

static NTSTATUS WINAPI
RtlMultiByteToUnicodeSize_Utf(
                PULONG BytesInUnicodeString,
          const CHAR   *MultiByteString,
                ULONG  BytesInMultiByteString)
{
	int len = WuTF_utf8_to_utf16(WUTF_INVALID_DROP,
	                             MultiByteString, BytesInMultiByteString,
	                             NULL, 0);
	*BytesInUnicodeString = len*2;
	return STATUS_SUCCESS;
}

static NTSTATUS WINAPI
RtlUnicodeToMultiByteSize_Utf(
                PULONG BytesInMultiByteString,
                PCWCH  UnicodeString,
                ULONG  BytesInUnicodeString)
{
	int len = WuTF_utf16_to_utf8(WUTF_INVALID_DROP,
	                             (uint16_t*)UnicodeString, BytesInUnicodeString/2,
	                             NULL, 0);
	*BytesInMultiByteString = len;
	return STATUS_SUCCESS;
}


// these four call RtlMultiByteToUnicodeSize (or the opposite) only if NlsMbCodePageTag is true
// if false, it just assumes 8bit length equals half the 16bit len
static ULONG WINAPI
RtlUnicodeStringToAnsiSize_Utf(
                PCUNICODE_STRING UnicodeString)
{
	ULONG len;
	RtlUnicodeToMultiByteSize_Utf(&len, UnicodeString->Buffer, UnicodeString->Length);
	return len+1;
}

static NTSTATUS WINAPI
RtlUnicodeStringToAnsiString_Utf(
                PANSI_STRING     DestinationString,
                PCUNICODE_STRING SourceString,
                BOOLEAN          AllocateDestinationString)
{
	NTSTATUS ret = STATUS_SUCCESS;
	int len = RtlUnicodeStringToAnsiSize_Utf(SourceString);
	
	DestinationString->Length = len-1;
	if (AllocateDestinationString)
	{
		DestinationString->MaximumLength = len;
		DestinationString->Buffer = (char*)RtlAllocateHeap_ptr(GetProcessHeap(), 0, len);
		if (!DestinationString->Buffer)
			return STATUS_NO_MEMORY;
	}
	
	if (DestinationString->MaximumLength < len)
	{
		if (!DestinationString->MaximumLength)
			return STATUS_BUFFER_OVERFLOW;
		DestinationString->Length = DestinationString->MaximumLength - 1;
		ret = STATUS_BUFFER_OVERFLOW;
	}
	
	RtlUnicodeToMultiByteN_Utf(DestinationString->Buffer, DestinationString->Length, NULL, SourceString->Buffer, SourceString->Length);
	DestinationString->Buffer[DestinationString->Length] = 0;
	if (DestinationString->MaximumLength < len)
		return STATUS_BUFFER_OVERFLOW;
	return STATUS_SUCCESS;
}

static ULONG WINAPI
RtlAnsiStringToUnicodeSize_Utf(
                PCANSI_STRING AnsiString)
{
	ULONG len;
	RtlMultiByteToUnicodeSize_Utf(&len, AnsiString->Buffer, AnsiString->Length);
	return len+sizeof(uint16_t);
}

static NTSTATUS WINAPI
RtlAnsiStringToUnicodeString_Utf(
                PUNICODE_STRING DestinationString,
                PCANSI_STRING   SourceString,
                BOOLEAN         AllocateDestinationString)
{
	ULONG len = RtlAnsiStringToUnicodeSize_Utf(SourceString);
	
	if (len >= 65536)
		return STATUS_BUFFER_OVERFLOW;
	DestinationString->Length = len-sizeof(uint16_t);
	
	if (AllocateDestinationString)
	{
		DestinationString->MaximumLength = len;
		DestinationString->Buffer = (wchar_t*)RtlAllocateHeap_ptr(GetProcessHeap(), 0, len);
		if (!DestinationString->Buffer)
			return STATUS_NO_MEMORY;
	}
	if (len > DestinationString->MaximumLength)
		return STATUS_BUFFER_OVERFLOW;
	
	RtlMultiByteToUnicodeN_Utf(DestinationString->Buffer, DestinationString->Length, NULL, SourceString->Buffer, SourceString->Length);
	DestinationString->Buffer[DestinationString->Length/sizeof(uint16_t)] = 0;
	return STATUS_SUCCESS;
}


// ignores invalid flags and parameters
static int WINAPI
MultiByteToWideChar_Utf(UINT CodePage, DWORD dwFlags,
                        LPCSTR lpMultiByteStr, int cbMultiByte,
                        LPWSTR lpWideCharStr, int cchWideChar)
{
	int ret = WuTF_utf8_to_utf16((dwFlags&MB_ERR_INVALID_CHARS) ? WUTF_INVALID_ABORT : WUTF_INVALID_DROP,
	                             lpMultiByteStr, cbMultiByte,
	                             (uint16_t*)lpWideCharStr, cchWideChar);
	
	if (ret<0)
	{
		if (ret == WUTF_E_INVALID) SetLastError(ERROR_NO_UNICODE_TRANSLATION);
		if (ret == WUTF_E_TRUNCATE) SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return 0;
	}
	return ret;
}

static int WINAPI
WideCharToMultiByte_Utf(UINT CodePage, DWORD dwFlags,
                        LPCWSTR lpWideCharStr, int cchWideChar,
                        LPSTR lpMultiByteStr, int cbMultiByte,
                        LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
{
	int ret = WuTF_utf16_to_utf8((dwFlags&MB_ERR_INVALID_CHARS) ? WUTF_INVALID_ABORT : WUTF_INVALID_DROP,
	                             (uint16_t*)lpWideCharStr, cchWideChar,
	                             lpMultiByteStr, cbMultiByte);
	
	if (ret<0)
	{
		if (ret == WUTF_E_INVALID) SetLastError(ERROR_NO_UNICODE_TRANSLATION);
		if (ret == WUTF_E_TRUNCATE) SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return 0;
	}
	return ret;
}


//https://sourceforge.net/p/predef/wiki/Architectures/
#if defined(_M_IX86) || defined(__i386__)
static void redirect_machine(LPBYTE victim, LPBYTE replacement)
{
	victim[0] = 0xE9; // jmp <offset from next instruction>
	*(LONG*)(victim+1) = replacement-victim-5;
}

#elif defined(_M_X64) || defined(__x86_64__)
static void redirect_machine(LPBYTE victim, LPBYTE replacement)
{
	// this destroys %rax, but ABI prohibits caller from using it anyways
	// https://msdn.microsoft.com/en-us/library/9z1stfyw.aspx
	victim[0] = 0x48; victim[1] = 0xB8;   // mov %rax, <64 bit constant>
	*(LPBYTE*)(victim+2) = replacement;
	victim[10] = 0xFF; victim[11] = 0xE0; // jmp %rax
}

#else
#error Not supported
#endif


typedef void(*WuTF_funcptr)();
static void WuTF_redirect_function(WuTF_funcptr victim, WuTF_funcptr replacement)
{
	DWORD prot;
	//it's generally discouraged to have W+X on the same page, but the alternative is risking
	// removing X from VirtualProtect or NtProtectVirtualMemory, and then I can't fix it.
	//it doesn't matter, anyways; we (should be) called so early no hostile input has been processed yet
	VirtualProtect((void*)victim, 64, PAGE_EXECUTE_READWRITE, &prot);
	redirect_machine((LPBYTE)victim, (LPBYTE)replacement);
	VirtualProtect((void*)victim, 64, prot, &prot);
}

void WuTF_enable()
{
	// TODO: windows 10 v1903 added proper utf8 support
	// https://docs.microsoft.com/en-us/windows/uwp/design/globalizing/use-utf8-code-page
	// doesn't seem settable via code, need to figure out how to get it into the makefiles
	// (or should I emit some funny sections via a .cpp?)
	// latest windows supporting the feature officially makes me a lot more comfortable hacking up old windowses
	
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	if (GetACP() == CP_UTF8) return;
	
	//it's safe to call this multiple times, that just replaces some bytes with their current values
	//if wutf becomes more complex, add a static bool initialized
#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#define REDIR(dll, func) WuTF_redirect_function((WuTF_funcptr)GetProcAddress(dll, STRINGIFY(func)), (WuTF_funcptr)func##_Utf)
	HMODULE ntdll = GetModuleHandle("ntdll.dll");
	RtlAllocateHeap_ptr = (decltype(RtlAllocateHeap)*)GetProcAddress(ntdll, "RtlAllocateHeap");
	
	//list of possibly relevant functions in ntdll.dll (pulled from 'strings ntdll.dll'):
	//some are documented at https://msdn.microsoft.com/en-us/library/windows/hardware/ff553354%28v=vs.85%29.aspx
	//many are implemented in terms of each other, often rooting in the ones I've hijacked
	//several others are just for legacy support and have few or no callers
	// RtlAnsiCharToUnicodeChar
	REDIR(ntdll, RtlAnsiStringToUnicodeSize);
	REDIR(ntdll, RtlAnsiStringToUnicodeString);
	// RtlAppendAsciizToString
	// RtlAppendPathElement
	// RtlAppendStringToString
	// RtlAppendUnicodeStringToString
	// RtlAppendUnicodeToString
	// RtlCreateUnicodeStringFromAsciiz
	// RtlMultiAppendUnicodeStringBuffer
	REDIR(ntdll, RtlMultiByteToUnicodeN);
	REDIR(ntdll, RtlMultiByteToUnicodeSize);
	// RtlOemStringToUnicodeSize
	// RtlOemStringToUnicodeString
	// RtlOemToUnicodeN
	// RtlRunDecodeUnicodeString
	// RtlRunEncodeUnicodeString
	REDIR(ntdll, RtlUnicodeStringToAnsiSize);
	REDIR(ntdll, RtlUnicodeStringToAnsiString);
	// RtlUnicodeStringToCountedOemString
	// RtlUnicodeStringToInteger
	// RtlUnicodeStringToOemSize
	// RtlUnicodeStringToOemString
	// RtlUnicodeToCustomCPN
	REDIR(ntdll, RtlUnicodeToMultiByteN);
	REDIR(ntdll, RtlUnicodeToMultiByteSize);
	// RtlUnicodeToOemN
	// RtlUpcaseUnicodeChar
	// RtlUpcaseUnicodeString
	// RtlUpcaseUnicodeStringToAnsiString
	// RtlUpcaseUnicodeStringToCountedOemString
	// RtlUpcaseUnicodeStringToOemString
	// RtlUpcaseUnicodeToCustomCPN
	// RtlUpcaseUnicodeToMultiByteN
	// RtlUpcaseUnicodeToOemN
	// RtlxAnsiStringToUnicodeSize
	// RtlxOemStringToUnicodeSize
	// RtlxUnicodeStringToAnsiSize
	// RtlxUnicodeStringToOemSize
	
	HMODULE kernel32 = GetModuleHandle("kernel32.dll");
	REDIR(kernel32, MultiByteToWideChar);
	REDIR(kernel32, WideCharToMultiByte);
#undef REDIR
}



void WuTF_args(int* argc_p, char** * argv_p)
{
	int i;
	
	int argc;
	LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	LPSTR* argv = (LPSTR*)HeapAlloc(GetProcessHeap(), 0, sizeof(LPSTR)*(argc+1));
	
	for (i=0;i<argc;i++)
	{
		int cb = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
		argv[i] = (char*)HeapAlloc(GetProcessHeap(), 0, cb);
		WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], cb, NULL, NULL);
	}
	argv[argc]=0;
	
	*argv_p = argv;
	*argc_p = argc;
}
#endif
