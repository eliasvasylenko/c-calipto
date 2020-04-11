#include <stdint.h>
#include <stdbool.h>
#include <uchar.h>
#include <string.h>

int32_t strlen32(const char32_t* s) {
	int32_t l = 0;
	while (s[l] != U'\0') l++;
	return l;
}

int32_t strcmp32(const char32_t* s1, const char32_t* s2) {
	for (int32_t i = 0; s1[i] != U'\0'; i++) {
		char32_t d = s2[i] - s1[i];
		if (d != 0)
			return d;
	}
	return 0;
}

int32_t strcnmp32(const char32_t* s1, const char32_t* s2, int32_t l) {
	for (int32_t i = 0; i < l; i++) {
		char32_t d = s2[i] - s1[i];
		if (d != 0)
			return d;
	}
	return 0;
}

int32_t c32stomb(char* pmb, const char32_t* s32) {
	int32_t size = 0;
	if (pmb == NULL) {
		while (*s32 != U'\0') 
			size += c32rtomb(NULL, *s32, NULL);
			s32++;
		}
	} else {
		while (*s32 != U'\0') 
			size += c32rtomb(pmb + size, *s32, NULL);
			s32++;
		}
	}
	return size;
}
