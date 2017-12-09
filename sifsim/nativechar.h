#pragma once

#include <string>

#if _WIN32

#include <codecvt>
#include <locale>

typedef wchar_t NativeChar;
typedef std::wstring NativeString;
#define REQUIRE_CHARSET_CONVERSION 1
#define NativeMain wmain
#define NativeFopen _wfopen

std::string ToUtf8(const NativeString & nativeStr) {
	return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(nativeStr);
}

NativeString ToNative(const std::string & str) {
	return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(str);
}

#else

typedef char NativeChar;
typedef std::string NativeString;
#define REQUIRE_CHARSET_CONVERSION 0
#define NativeFopen std::fopen

std::string ToUtf8(const NativeString & nativeStr) {
	return nativeStr;
}

NativeString ToNative(const std::string & str) {
	return str;
}

#endif
