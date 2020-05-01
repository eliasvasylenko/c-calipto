#include <uchar.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/umachine.h>
#include <unicode/ucnv.h>

#include "c-calipto/stringref.h"

strref c_strrefn(const UConverter* conv, int32_t size, const char* chars) {
	return (strref){
		CSTR,
		size,
		conv,
		chars
	};
}

strref c_strref(const UConverter* conv, const char* chars) {
	return c_strrefn(conv, strlen(chars), chars);
}

int32_t maxlen(strref s) {
	switch (s.type) {
		case CSTR:
			return size * 2;
		case USTR:
			return size;
	}
}

UChar* malloc_strrefcpy(strref s, int32_t* l) {
	int maxlen = maxlen(s);
	UChar* us = malloc(sizeof(UChar) * maxlen + 1);
	int len;
	switch (s.type) {
		case CSTR:
			UErrorCode error = 0;
			len = ucnv_toUChars(s.conv,
					us, maxlen,
					s.c_chars, s.size,
					&error);
			break;
		case USTR:
			len = s.size;
			u_strncpy(us, s.u_chars, s.size);
			break;
	}
	if (len < maxlen) {
		UChar* uso = us;
		us = malloc(sizeof(UChar) * len + 1);
		u_strcpy(us, uso, len);
		free(uso);
	}
	us[len] = u'\0';
	if (l != NULL) {
		*l = len;
	}
	return us;
}

