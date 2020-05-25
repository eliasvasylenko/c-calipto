#include <stdlib.h>
#include <string.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>
#include <unicode/ucnv.h>

#include "c-calipto/stringref.h"

strref c_strnref(UConverter* conv, int32_t size, const char* chars) {
	return (strref){
		CSTR,
		size,
		conv,
		.c_chars=chars
	};
}

strref c_strref(UConverter* conv, const char* chars) {
	return c_strnref(conv, strlen(chars), chars);
}

strref u_strnref(int32_t size, const UChar* chars) {
	return (strref){
		USTR,
		size,
		NULL,
		.u_chars=chars
	};
}

strref u_strref(const UChar* chars) {
	return u_strnref(u_strlen(chars), chars);
}

uint32_t strref_maxlen(strref s) {
	switch (s.type) {
		case CSTR:
			return s.size * 2;
		case USTR:
			return s.size;
	}
}

uint32_t strref_cpy(int destSize, UChar* dest, strref s) {
	int32_t len;
	switch (s.type) {
		case CSTR:
			;
			UErrorCode error = 0;
			len = ucnv_toUChars(s.conv,
					dest, destSize,
					s.c_chars, s.size,
					&error);
			break;
		case USTR:
			;
			len = s.size;
			u_strncpy(dest, s.u_chars, len < destSize ? len : destSize);
			break;
	}
	return len;
}

