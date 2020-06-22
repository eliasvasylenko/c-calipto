#include <stdlib.h>
#include <string.h>

#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>
#include <unicode/ucnv.h>

#include "c-ohvu/io/stringref.h"

typedef ovio_strref strref;

strref ovio_c_strnref(UConverter* conv, int32_t size, const char* chars) {
	return (strref){
		OVIO_CSTR,
		size,
		conv,
		.c_chars=chars
	};
}

strref ovio_c_strref(UConverter* conv, const char* chars) {
	return ovio_c_strnref(conv, strlen(chars), chars);
}

strref ovio_u_strnref(int32_t size, const UChar* chars) {
	return (strref){
		OVIO_USTR,
		size,
		NULL,
		.u_chars=chars
	};
}

strref ovio_u_strref(const UChar* chars) {
	return ovio_u_strnref(u_strlen(chars), chars);
}

uint32_t ovio_strref_maxlen(strref s) {
	switch (s.type) {
		case OVIO_CSTR:
			return s.size * 2;
		case OVIO_USTR:
			return s.size;
	}
}

uint32_t ovio_strref_cpy(int destSize, UChar* dest, strref s) {
	int32_t len;
	switch (s.type) {
		case OVIO_CSTR:
			;
			UErrorCode error = 0;
			len = ucnv_toUChars(s.conv,
					dest, destSize,
					s.c_chars, s.size,
					&error);
			break;
		case OVIO_USTR:
			;
			len = s.size;
			u_strncpy(dest, s.u_chars, len < destSize ? len : destSize);
			break;
	}
	return len;
}

