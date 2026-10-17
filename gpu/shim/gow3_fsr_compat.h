#pragma once

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <mutex>
#include <new>

#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

inline int gow3_swprintf_s(wchar_t *buffer, std::size_t size, const wchar_t *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int result = std::vswprintf(buffer, size, format, arguments);
	va_end(arguments);
	if (result < 0 && buffer && size) {
		buffer[0] = L'\0';
	}
	return result;
}

#ifndef swprintf_s
#define swprintf_s gow3_swprintf_s
#endif