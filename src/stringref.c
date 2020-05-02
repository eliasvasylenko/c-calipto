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

int32_t maxlen(strref s) {
	switch (s.type) {
		case CSTR:
			return s.size * 2;
		case USTR:
			return s.size;
	}
}

UChar* malloc_strrefcpy(strref s, int32_t* l) {
	int32_t max = maxlen(s);
	UChar* us = malloc(sizeof(UChar) * (max + 1));
	int32_t len;
	switch (s.type) {
		case CSTR:
			;
			UErrorCode error = 0;
			len = ucnv_toUChars(s.conv,
					us, max,
					s.c_chars, s.size,
					&error);
			break;
		case USTR:
			;
			len = s.size;
			u_strncpy(us, s.u_chars, s.size);
			break;
	}
	if (len < max) {
		UChar* uso = us;
		us = malloc(sizeof(UChar) * (len + 1));
		u_strcpy(us, uso, len);
		free(uso);
	}
	us[len] = u'\0';
	if (l != NULL) {
		*l = len;
	}
	return us;
}

